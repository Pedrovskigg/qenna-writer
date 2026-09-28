#include "ElementCreateDialog.h"

#include "ImageCropDialog.h"
#include "TimelineTracksTypes.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCoreApplication>
#include <QEasingCurve>
#include <QEnterEvent>
#include <QGraphicsDropShadowEffect>
#include <QLinearGradient>
#include <QVariantAnimation>
#include <QByteArray>
#include <QCheckBox>
#include <QFrame>
#include <QGridLayout>
#include <QScreen>
#include <QStyle>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

using namespace Tracks;

namespace {

// Foto de cenário e objeto: quadrada ao lado do nome. (O personagem usa o
// pôster, ver CharacterPoster.) A imagem salva é sempre quadrada: é o avatar
// no app inteiro.
constexpr int kSquare = 124;

QPixmap pixmapFromDataUrl(const QString& dataUrl) {
    if (dataUrl.isEmpty()) return QPixmap();
    const int comma = dataUrl.indexOf(QLatin1Char(','));
    if (comma < 0) return QPixmap();
    const QByteArray raw = QByteArray::fromBase64(dataUrl.mid(comma + 1).toLatin1());
    QPixmap pm;
    pm.loadFromData(raw);
    return pm;
}

struct RoleOpt { const char* id; const char* label; const char* what; };
// A lista inteira, na ordem da grade (duas colunas): os oito papéis, "Outro…"
// (nome livre) e "Sem papel" (decide depois).
const RoleOpt kRoles[] = {
    { "PROTAGONISTA",   QT_TRANSLATE_NOOP("ElementCreateDialog", "Protagonista"),   QT_TRANSLATE_NOOP("ElementCreateDialog", "quem conduz a história") },
    { "DEUTERAGONISTA", QT_TRANSLATE_NOOP("ElementCreateDialog", "Deuteragonista"), QT_TRANSLATE_NOOP("ElementCreateDialog", "segundo em importância") },
    { "ANTAGONISTA",    QT_TRANSLATE_NOOP("ElementCreateDialog", "Antagonista"),    QT_TRANSLATE_NOOP("ElementCreateDialog", "quem se opõe") },
    { "COADJUVANTE",    QT_TRANSLATE_NOOP("ElementCreateDialog", "Coadjuvante"),    QT_TRANSLATE_NOOP("ElementCreateDialog", "apoia os principais") },
    { "MENTOR",         QT_TRANSLATE_NOOP("ElementCreateDialog", "Mentor"),         QT_TRANSLATE_NOOP("ElementCreateDialog", "ensina, guia, empurra") },
    { "CONTRAPONTO",    QT_TRANSLATE_NOOP("ElementCreateDialog", "Contraponto"),    QT_TRANSLATE_NOOP("ElementCreateDialog", "espelho do protagonista") },
    { "TRICKSTER",      QT_TRANSLATE_NOOP("ElementCreateDialog", "Trickster"),      QT_TRANSLATE_NOOP("ElementCreateDialog", "o imprevisível") },
    { "FIGURANTE",      QT_TRANSLATE_NOOP("ElementCreateDialog", "Figurante"),      QT_TRANSLATE_NOOP("ElementCreateDialog", "aparece de passagem") },
    { "OUTRO",          QT_TRANSLATE_NOOP("ElementCreateDialog", "Outro…"),         QT_TRANSLATE_NOOP("ElementCreateDialog", "um papel com nome seu") },
    { "",               QT_TRANSLATE_NOOP("ElementCreateDialog", "Sem papel"),      QT_TRANSLATE_NOOP("ElementCreateDialog", "decide depois") },
};
constexpr int kRoleCount = 8;   // os papéis de verdade (antes de "Outro…" e "Sem papel")

QIcon chevronIcon(const QColor& c)
{
    QPixmap pm(28, 28);
    pm.setDevicePixelRatio(2.0);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(c, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    QPainterPath path;
    path.moveTo(3.5, 5.5); path.lineTo(7, 9); path.lineTo(10.5, 5.5);
    p.drawPath(path);
    return QIcon(pm);
}

QIcon crossIcon(const QColor& c)
{
    QPixmap pm(28, 28);
    pm.setDevicePixelRatio(2.0);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(c, 1.6, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(4, 4), QPointF(10, 10));
    p.drawLine(QPointF(10, 4), QPointF(4, 10));
    return QIcon(pm);
}

// O botão da foto quando ainda não tem foto: um quadrado com uma silhueta,
// apagado; acende (borda na cor de destaque, "+" no canto) com o mouse em cima.
class SilhouetteButton : public QAbstractButton {
public:
    explicit SilhouetteButton(QWidget* parent) : QAbstractButton(parent)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        m_anim = new QVariantAnimation(this);
        m_anim->setDuration(150);
        connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
            m_hover = v.toReal();
            update();
        });
    }

protected:
    void enterEvent(QEnterEvent* e) override { animateTo(1.0); QAbstractButton::enterEvent(e); }
    void leaveEvent(QEvent* e) override { animateTo(0.0); QAbstractButton::leaveEvent(e); }

    void paintEvent(QPaintEvent*) override
    {
        const Palette pal = Palette::current();
        const qreal h = m_hover;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setOpacity(0.5 + 0.5 * h);
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        QPainterPath box;
        box.addRoundedRect(r, 7, 7);
        p.fillPath(box, mix(alpha(pal.accent, 0.09), alpha(pal.ink, 0.05), h));

        // silhueta (cabeça + ombros) encostada no pé do quadrado
        p.save();
        p.setClipPath(box);
        const qreal side = width() * 0.795;   // a figura ocupa ~80% do quadrado
        const qreal s = side / 250.0;
        p.translate((width() - side) / 2.0, height() - side);
        p.scale(s, s);
        QPainterPath fig;
        fig.addEllipse(QPointF(125, 92), 52, 52);
        fig.moveTo(28, 250);
        fig.cubicTo(32, 184, 72, 148, 125, 148);
        fig.cubicTo(178, 148, 218, 184, 222, 250);
        fig.closeSubpath();
        p.fillPath(fig, mix(alpha(pal.accent, 0.5), alpha(pal.ink, 0.4), h));
        p.restore();

        QPen pen(mix(pal.accent, alpha(pal.ink, 0.32), h), 1);
        pen.setStyle(h < 0.5 ? Qt::DashLine : Qt::SolidLine);
        p.setPen(pen);
        p.drawPath(box);

        if (h > 0.01) {
            p.setOpacity(h);
            const QPointF c(width() - 10.5, 10.5);
            p.setPen(Qt::NoPen);
            p.setBrush(pal.accent);
            p.drawEllipse(c, 7, 7);
            p.setPen(QPen(pal.page, 1.5, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(c - QPointF(3.2, 0), c + QPointF(3.2, 0));
            p.drawLine(c - QPointF(0, 3.2), c + QPointF(0, 3.2));
        }
    }

private:
    void animateTo(qreal v)
    {
        m_anim->stop();
        m_anim->setStartValue(m_hover);
        m_anim->setEndValue(v);
        m_anim->start();
    }
    QVariantAnimation* m_anim = nullptr;
    qreal m_hover = 0;
};

// Uma linha só; se não couber na largura, termina em "…" e mostra o texto
// inteiro ao passar o mouse.
class OneLineLabel : public QLabel {
public:
    OneLineLabel(const QString& text, QWidget* parent) : QLabel(text, parent) {}
    QSize minimumSizeHint() const override { return QSize(0, QLabel::minimumSizeHint().height()); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setPen(palette().color(foregroundRole()));
        const QString shown = fontMetrics().elidedText(text(), Qt::ElideRight, contentsRect().width());
        setToolTip(shown == text() ? QString() : text());
        p.drawText(contentsRect(), int(alignment()) | Qt::TextSingleLine, shown);
    }
};

} // namespace

// Topo da folha de personagem ("pôster em pé"). Sem foto: uma faixa baixa com
// o quadrado da silhueta à esquerda e o papel + nome ao lado. Com foto: o topo
// cresce, a foto ocupa tudo de ponta a ponta, escurecida embaixo, e o papel e
// o nome ficam em branco por cima.
class CharacterPoster : public QWidget {
public:
    static constexpr int kStripH = 100;
    // Com foto, o topo é quadrado como a foto guardada (298 = os 300 da folha
    // menos a borda): mostra exatamente o recorte que a pessoa escolheu, sem
    // cortar de novo. Só encolhe se a tela for baixa demais pra folha caber.
    static constexpr int kPosterH = 298;

    CharacterPoster(QLineEdit* name, QLabel* role, QWidget* parent)
        : QWidget(parent), m_name(name), m_role(role)
    {
        setFixedHeight(kStripH);
        m_name->setParent(this);
        m_role->setParent(this);
        m_eyebrow = SheetDialog::sectionLabel(QString(), this);
        m_close = new QToolButton(this);
        m_close->setObjectName(QStringLiteral("sheetX"));
        m_close->setFixedSize(24, 24);
        m_close->setCursor(Qt::PointingHandCursor);
        m_close->setFocusPolicy(Qt::NoFocus);
        m_close->setToolTip(QCoreApplication::translate("SheetDialog", "Cancelar (Esc)"));
        m_add = new SilhouetteButton(this);
        m_add->setFixedSize(58, 58);
        m_add->setToolTip(QCoreApplication::translate("ElementCreateDialog", "Adicionar foto (opcional)"));
        m_swap = pillLink(QCoreApplication::translate("ElementCreateDialog", "trocar"));
        m_remove = pillLink(QCoreApplication::translate("ElementCreateDialog", "remover"));
        m_shadow = new QGraphicsDropShadowEffect(m_name);
        m_shadow->setBlurRadius(12);
        m_shadow->setOffset(0, 1);
        m_shadow->setColor(QColor(0, 0, 0, 115));
        m_shadow->setEnabled(false);
        m_name->setGraphicsEffect(m_shadow);
        m_anim = new QVariantAnimation(this);
        m_anim->setDuration(260);
        m_anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
            const int h = v.toInt();
            // centralizada (modal): cresce pros dois lados e continua no meio.
            // Ao lado da gaveta: cresce só pra baixo, o topo fica alinhado.
            if (QWidget* w = window(); w && w->isVisible() && w->isModal())
                w->move(w->x(), w->y() - (h - height()) / 2);
            setFixedHeight(h);
        });
        applyLook();
    }

    QAbstractButton* addButton() const { return m_add; }
    QToolButton* swapButton() const { return m_swap; }
    QToolButton* removeButton() const { return m_remove; }
    QToolButton* closeButton() const { return m_close; }
    void setEyebrow(const QString& t) { m_eyebrow->setText(t.toUpper()); m_eyebrow->adjustSize(); relayout(); }

    void setPhoto(const QPixmap& pm)
    {
        const bool had = !m_photo.isNull();
        m_photo = pm;
        applyLook();
        int target = kStripH;
        if (!pm.isNull()) {
            target = kPosterH;
            if (QScreen* scr = screen()) {
                const int rest = window()->sizeHint().height() - height();
                target = qBound(kStripH, scr->availableGeometry().height() - rest - 40, kPosterH);
            }
        }
        if (had != !pm.isNull() && isVisible()) {
            m_anim->stop();
            m_anim->setStartValue(height());
            m_anim->setEndValue(target);
            m_anim->start();
        } else {
            setFixedHeight(target);
        }
        update();
    }

    // Cores de quem fica por cima: branco sobre a foto, as da folha na faixa.
    void applyLook()
    {
        const Palette pal = Palette::current();
        const bool on = !m_photo.isNull();
        m_close->setIcon(crossIcon(on ? QColor(255, 255, 255) : pal.dim));
        m_eyebrow->setStyleSheet(on ? QStringLiteral("color: rgba(255,255,255,0.85); background: transparent;") : QString());
        m_role->setStyleSheet(QStringLiteral("background: transparent; color: %1;")
                                  .arg(on ? QStringLiteral("rgba(255,255,255,0.82)") : pal.dim.name()));
        m_name->setStyleSheet(on ? QStringLiteral("QLineEdit { background: transparent; border: none; color: #ffffff;"
                                                  " selection-background-color: rgba(255,255,255,0.3); padding: 0; }")
                                 : QString());
        QPalette np = m_name->palette();
        np.setColor(QPalette::PlaceholderText, on ? QColor(255, 255, 255, 178) : mix(pal.ink, pal.page, 0.42));
        m_name->setPalette(np);
        m_shadow->setEnabled(on);
        // na faixa o nome divide a largura com o quadrado: um pouco menor
        m_name->setFont(serifFont(on ? 23 : 19));
        m_add->setVisible(!on);
        m_swap->setVisible(on);
        m_remove->setVisible(on);
        relayout();
    }

    void relayout()
    {
        const int W = width(), H = height();
        m_eyebrow->adjustSize();
        m_eyebrow->move(15, 14);
        m_close->move(W - 9 - 24, 8);
        const bool on = !m_photo.isNull();
        const int left = on ? 16 : 86;
        const int nameH = m_name->height();
        m_name->setGeometry(left, H - 9 - nameH, W - left - (on ? 16 : 12), nameH);
        m_role->adjustSize();
        m_role->move(left + 1, m_name->y() - m_role->height() + 3);
        m_add->move(16, 31);
        if (on) {
            m_remove->adjustSize();
            m_swap->adjustSize();
            m_remove->move(m_close->x() - 6 - m_remove->width(), 10);
            m_swap->move(m_remove->x() - 5 - m_swap->width(), 10);
        }
    }

protected:
    void resizeEvent(QResizeEvent* e) override { QWidget::resizeEvent(e); relayout(); }

    void paintEvent(QPaintEvent*) override
    {
        const Palette pal = Palette::current();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        // cantos de cima acompanham a folha; os de baixo são retos
        const qreal R = 9, W = width(), H = height();
        QPainterPath clip;
        clip.moveTo(0, H);
        clip.lineTo(0, R);
        clip.arcTo(QRectF(0, 0, 2 * R, 2 * R), 180, -90);
        clip.lineTo(W - R, 0);
        clip.arcTo(QRectF(W - 2 * R, 0, 2 * R, 2 * R), 90, -90);
        clip.lineTo(W, H);
        clip.closeSubpath();
        p.setClipPath(clip);
        if (m_photo.isNull()) {
            p.fillRect(rect(), mix(pal.app, pal.page, 0.55));
            return;
        }
        const qreal dpr = devicePixelRatioF();
        const QPixmap sc = m_photo.scaled(QSizeF(W * dpr, H * dpr).toSize(), Qt::KeepAspectRatioByExpanding,
                                          Qt::SmoothTransformation);
        // o rosto costuma estar no terço de cima: o corte vertical puxa pra lá
        p.drawPixmap(QRectF(0, 0, W, H), sc,
                     QRectF((sc.width() - W * dpr) / 2, (sc.height() - H * dpr) * 0.22, W * dpr, H * dpr));
        QLinearGradient g(0, 0, 0, H);
        g.setColorAt(0.0, QColor(0, 0, 0, 77));
        g.setColorAt(0.22, QColor(0, 0, 0, 0));
        g.setColorAt(0.45, QColor(0, 0, 0, 0));
        g.setColorAt(1.0, QColor(0, 0, 0, 199));
        p.fillRect(rect(), g);
    }

private:
    QToolButton* pillLink(const QString& text)
    {
        auto* b = new QToolButton(this);
        b->setText(text);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        b->setFont(uiFont(10));
        b->setFixedHeight(20);
        b->setStyleSheet(QStringLiteral(
            "QToolButton { background: rgba(0,0,0,0.45); color: #ffffff; border: none; border-radius: 10px; padding: 0 8px; }"
            "QToolButton:hover { background: rgba(0,0,0,0.62); }"));
        return b;
    }

    QLineEdit* m_name;
    QLabel* m_role;
    QLabel* m_eyebrow = nullptr;
    QToolButton* m_close = nullptr;
    SilhouetteButton* m_add = nullptr;
    QToolButton* m_swap = nullptr;
    QToolButton* m_remove = nullptr;
    QGraphicsDropShadowEffect* m_shadow = nullptr;
    QVariantAnimation* m_anim = nullptr;
    QPixmap m_photo;
};

ElementCreateDialog::ElementCreateDialog(const QString& elementType, QWidget* parent,
                                         const QVector<SheetTemplate>& sheetTemplates)
    : SheetDialog(parent, elementType == QStringLiteral("character") ? 300 : 460)
    , m_elementType(elementType)
    , m_sheetTemplates(sheetTemplates)
{
    setObjectName(QStringLiteral("elementCreateDialog"));
    buildUi();
}

void ElementCreateDialog::buildUi()
{
    const bool character = m_elementType == QStringLiteral("character");
    const bool visual = character || m_elementType == QStringLiteral("setting")
                     || m_elementType == QStringLiteral("object");
    setEyebrow(character ? tr("Novo personagem")
               : m_elementType == QStringLiteral("setting") ? tr("Novo cenário")
               : m_elementType == QStringLiteral("object") ? tr("Novo objeto") : tr("Novo documento"));
    QWidget* c = card();

    const QString namePh = character ? tr("Nome do personagem")
                         : m_elementType == QStringLiteral("setting") ? tr("Nome do cenário")
                         : m_elementType == QStringLiteral("object") ? tr("Nome do objeto") : tr("Nome do documento");
    QVBoxLayout* fields = body();
    if (character) {
        // pôster em pé: faixa com a silhueta (sem foto) ou a foto de ponta a
        // ponta com o nome por cima
        m_titleEdit = titleEdit(c, false, 24);
        // o topo já diz "NOVO PERSONAGEM"; "Nome do personagem" não cabe ao lado do quadrado
        m_titleEdit->setPlaceholderText(tr("Nome"));
        m_roleTag = new QLabel(c);
        m_roleTag->setObjectName(QStringLiteral("sheetPosterRole"));
        QFont tf = uiFont(8.5, QFont::DemiBold);
        tf.setLetterSpacing(QFont::PercentageSpacing, 113);
        tf.setCapitalization(QFont::AllUppercase);
        m_roleTag->setFont(tf);
        m_poster = new CharacterPoster(m_titleEdit, m_roleTag, c);
        replaceHeader(m_poster);
        m_poster->setEyebrow(tr("Novo personagem"));
        connect(m_poster->closeButton(), &QToolButton::clicked, this, &QDialog::reject);
        connect(m_poster->addButton(), &QAbstractButton::clicked, this, [this]() { pickImage(); });
        connect(m_poster->swapButton(), &QToolButton::clicked, this, [this]() { pickImage(); });
        connect(m_poster->removeButton(), &QToolButton::clicked, this, [this]() { m_imageDataUrl.clear(); updatePreview(); });
        body()->setContentsMargins(16, 14, 16, 4);
        body()->setSpacing(9);

        m_aliasesEdit = field(c);
        m_aliasesEdit->setPlaceholderText(tr("Apelidos (Mari, a herdeira)"));
        m_aliasesEdit->setToolTip(tr("O detector de presença também procura esses nomes no texto"));
        body()->addWidget(m_aliasesEdit);
    } else if (visual) {
        // foto (clicar escolhe) + nome grande ao lado
        auto* row = new QHBoxLayout;
        row->setSpacing(16);
        auto* photoCol = new QVBoxLayout;
        photoCol->setSpacing(4);
        m_photoBtn = new QToolButton(c);
        m_photoBtn->setObjectName(QStringLiteral("sheetPhoto"));
        m_photoBtn->setFixedSize(kSquare, kSquare);
        m_photoBtn->setCursor(Qt::PointingHandCursor);
        m_photoBtn->setFocusPolicy(Qt::NoFocus);
        m_photoBtn->setToolTip(tr("Escolher foto (opcional)"));
        connect(m_photoBtn, &QToolButton::clicked, this, [this]() { pickImage(); });
        photoCol->addWidget(m_photoBtn);
        m_removePhoto = new QToolButton(c);
        m_removePhoto->setObjectName(QStringLiteral("sheetLink"));
        m_removePhoto->setText(tr("remover"));
        m_removePhoto->setCursor(Qt::PointingHandCursor);
        m_removePhoto->setFocusPolicy(Qt::NoFocus);
        connect(m_removePhoto, &QToolButton::clicked, this, [this]() { m_imageDataUrl.clear(); updatePreview(); });
        photoCol->addWidget(m_removePhoto, 0, Qt::AlignHCenter);
        photoCol->addStretch(1);
        row->addLayout(photoCol);

        auto* right = new QVBoxLayout;
        right->setSpacing(8);
        m_titleEdit = titleEdit(c, false, 24);
        m_titleEdit->setPlaceholderText(namePh);
        right->addWidget(m_titleEdit);
        right->addStretch(1);
        row->addLayout(right, 1);
        body()->addLayout(row);
    } else {
        m_titleEdit = titleEdit(c, true, 27);
        m_titleEdit->setPlaceholderText(namePh);
        body()->addWidget(m_titleEdit);
    }
    confirmOnEnter(m_titleEdit);
    if (m_aliasesEdit) confirmOnEnter(m_aliasesEdit);

    if (character) {
        // papel
        auto* g = new QVBoxLayout;
        g->setSpacing(5);
        g->addWidget(sectionLabel(tr("Papel"), c));
        // a linha: "Coadjuvante · apoia os principais" + seta
        m_rolePick = new QPushButton(c);
        m_rolePick->setObjectName(QStringLiteral("sheetPick"));
        m_rolePick->setCursor(Qt::PointingHandCursor);
        m_rolePick->setFixedHeight(28);
        m_rolePick->setAutoDefault(false);
        {
            auto* h = new QHBoxLayout(m_rolePick);
            h->setContentsMargins(9, 0, 7, 0);
            m_rolePickText = new QLabel(m_rolePick);
            m_rolePickText->setObjectName(QStringLiteral("sheetPickText"));
            m_rolePickText->setTextFormat(Qt::RichText);
            m_rolePickText->setAttribute(Qt::WA_TransparentForMouseEvents);
            h->addWidget(m_rolePickText, 1);
            auto* chev = new QLabel(m_rolePick);
            chev->setPixmap(chevronIcon(Palette::current().dim).pixmap(12, 12));
            chev->setAttribute(Qt::WA_TransparentForMouseEvents);
            h->addWidget(chev);
        }
        g->addWidget(m_rolePick);
        // a lista, que abre na própria folha: duas colunas, nome + o que é
        m_roleList = new QFrame(c);
        m_roleList->setObjectName(QStringLiteral("sheetRoleList"));
        auto* grid = new QGridLayout(m_roleList);
        grid->setContentsMargins(3, 3, 3, 3);
        grid->setSpacing(2);
        const int nOpts = int(sizeof(kRoles) / sizeof(kRoles[0]));
        for (int i = 0; i < nOpts; ++i) {
            auto* cell = new QPushButton(m_roleList);
            cell->setObjectName(QStringLiteral("sheetRoleCell"));
            cell->setCursor(Qt::PointingHandCursor);
            cell->setAutoDefault(false);
            cell->setFocusPolicy(Qt::NoFocus);
            cell->setFixedHeight(37);
            cell->setProperty("roleId", QString::fromLatin1(kRoles[i].id));
            auto* v = new QVBoxLayout(cell);
            v->setContentsMargins(6, 3, 6, 3);
            v->setSpacing(0);
            auto* name = new QLabel(tr(kRoles[i].label), cell);
            name->setObjectName(QStringLiteral("sheetRoleName"));
            name->setFont(serifFont(12));
            name->setAttribute(Qt::WA_TransparentForMouseEvents);
            auto* what = new OneLineLabel(tr(kRoles[i].what), cell);
            what->setObjectName(QStringLiteral("sheetDim"));
            what->setFont(uiFont(9.5));
            what->setAttribute(Qt::WA_TransparentForMouseEvents);
            v->addWidget(name);
            v->addWidget(what);
            grid->addWidget(cell, i / 2, i % 2);
            const QString id = QString::fromLatin1(kRoles[i].id);
            connect(cell, &QPushButton::clicked, this, [this, id]() {
                setRole(id);
                m_roleList->hide();
                if (id == QLatin1String("OUTRO")) m_roleCustom->setFocus();
                adjustSize();
            });
        }
        m_roleList->hide();
        g->addWidget(m_roleList);
        connect(m_rolePick, &QPushButton::clicked, this, [this]() {
            m_roleList->setVisible(!m_roleList->isVisible());
            adjustSize();
        });
        m_roleCustom = field(c);
        m_roleCustom->setPlaceholderText(tr("Qual papel?"));
        m_roleCustom->hide();
        confirmOnEnter(m_roleCustom);
        connect(m_roleCustom, &QLineEdit::textChanged, this, [this]() { setRole(QStringLiteral("OUTRO")); });
        g->addWidget(m_roleCustom);
        fields->addLayout(g);
        setRole(QString());

        m_narratorCheck = new QCheckBox(tr("Narrador (voz em 1ª pessoa)"), c);
        m_narratorCheck->setObjectName(QStringLiteral("sheetChk"));
        fields->addWidget(m_narratorCheck);

        // trilha na Timeline
        auto* gt = new QVBoxLayout;
        gt->setSpacing(5);
        gt->addWidget(sectionLabel(tr("Trilha na Timeline"), c));
        auto* tr1 = new QHBoxLayout;
        tr1->setSpacing(5);
        m_trackGroup = new QButtonGroup(this);
        const QString tlabels[3] = { tr("Automático"), tr("Sempre"), tr("Nunca") };
        const QString ttips[3] = { tr("A Timeline decide pelo papel do personagem"),
                                   tr("Sempre ganha uma trilha na Timeline"),
                                   tr("Nunca ganha trilha na Timeline") };
        for (int i = 0; i < 3; ++i) {
            auto* b = pill(tlabels[i], c);
            b->setToolTip(ttips[i]);
            m_trackGroup->addButton(b, i);
            tr1->addWidget(b);
        }
        tr1->addStretch(1);
        m_trackGroup->button(0)->setChecked(true);
        gt->addLayout(tr1);
        fields->addLayout(gt);

        // tipo de página (documento livre é o padrão: pedido dele, não gosta de Ficha pré-selecionada)
        auto* gp = new QVBoxLayout;
        gp->setSpacing(5);
        gp->addWidget(sectionLabel(tr("Página"), c));
        auto* pr = new QHBoxLayout;
        pr->setSpacing(5);
        m_pageGroup = new QButtonGroup(this);
        auto* freeBtn = pill(tr("Documento livre"), c);
        auto* sheet = pill(tr("Ficha (campos prontos)"), c);
        m_pageGroup->addButton(freeBtn, 0);
        m_pageGroup->addButton(sheet, 1);
        freeBtn->setChecked(true);
        pr->addWidget(freeBtn);
        pr->addWidget(sheet);
        pr->addStretch(1);
        gp->addLayout(pr);
        fields->addLayout(gp);
        connect(m_pageGroup, &QButtonGroup::idClicked, this, [this]() { updatePageTypeUi(); });

        // modelo de ficha (só com Ficha e havendo modelos)
        m_templateBox = new QWidget(c);
        auto* gm = new QVBoxLayout(m_templateBox);
        gm->setContentsMargins(0, 0, 0, 0);
        gm->setSpacing(5);
        gm->addWidget(sectionLabel(tr("Modelo de ficha"), m_templateBox));
        m_templateGroup = new QButtonGroup(this);
        QHBoxLayout* mr = nullptr;
        for (int i = 0; i <= m_sheetTemplates.size(); ++i) {
            if (i % 2 == 0) { mr = new QHBoxLayout; mr->setSpacing(5); gm->addLayout(mr); }
            auto* b = pill(i == 0 ? tr("Vazio (padrão)") : m_sheetTemplates[i - 1].name, m_templateBox);
            m_templateGroup->addButton(b, i);
            mr->addWidget(b);
        }
        if (mr) mr->addStretch(1);
        m_templateGroup->button(0)->setChecked(true);
        fields->addWidget(m_templateBox);
        updatePageTypeUi();
    }

    QPushButton* ok = addFooter(tr("Criar"), tr("cria"));
    auto sync = [this, ok]() { ok->setEnabled(!m_titleEdit->text().trimmed().isEmpty()); };
    connect(m_titleEdit, &QLineEdit::textChanged, this, sync);
    sync();

    if (character) {
        // A folha de personagem abre ao lado da gaveta, com o editor à vista:
        // 25% menor que as outras folhas (o texto encolhe menos, pra continuar legível).
        for (QLabel* l : c->findChildren<QLabel*>(QStringLiteral("sheetSection"))) {
            QFont f = uiFont(8.5, QFont::DemiBold);
            f.setLetterSpacing(QFont::PercentageSpacing, 113);
            l->setFont(f);
        }
        for (QToolButton* b : c->findChildren<QToolButton*>(QStringLiteral("sheetType"))) {
            b->setFixedHeight(22);
            b->setFont(uiFont(10.5));
        }
        for (QLineEdit* e : c->findChildren<QLineEdit*>(QStringLiteral("sheetFld"))) {
            e->setFixedHeight(27);
            e->setFont(uiFont(11.5));
        }
        for (QPushButton* b : c->findChildren<QPushButton*>())
            if (b->objectName() == QLatin1String("sheetBtn") || b->objectName() == QLatin1String("sheetPri"))
                b->setFixedHeight(26);
        for (QLabel* k : c->findChildren<QLabel*>(QStringLiteral("sheetKbd"))) {
            k->setFont(monoFont(9.5));
            k->setFixedHeight(16);
        }
    }

    const Palette pal = Palette::current();
    applySheetTheme(QStringLiteral(
        "QToolButton#sheetPhoto { background: %1; border: 1px dashed %2; border-radius: 10px; color: %3; font-size: 12.5px; }"
        "QToolButton#sheetPhoto:hover { border-color: %4; color: %5; }"
        "QToolButton#sheetPhoto[hasPhoto=\"true\"] { border: none; padding: 0; }"
        "QPushButton#sheetPick { background: %1; border: 1px solid %6; border-radius: 6px; text-align: left; }"
        "QPushButton#sheetPick:hover { border-color: %2; }"
        "QLabel#sheetPickText, QLabel#sheetRoleName { background: transparent; color: %5; font-size: 12.5px; }"
        "QFrame#sheetRoleList { background: %1; border: 1px solid %6; border-radius: 8px; }"
        "QPushButton#sheetRoleCell { background: transparent; border: 1px solid transparent; border-radius: 6px; text-align: left; }"
        "QPushButton#sheetRoleCell:hover { background: %7; }"
        "QPushButton#sheetRoleCell[sel=\"true\"] { background: %8; border-color: %9; }")
        .arg(mix(pal.app, pal.page, 0.55).name(), alpha(pal.ink, 0.35).name(QColor::HexArgb),
             pal.dim.name(), pal.accent.name(), pal.bright.name(), pal.border.name(),
             alpha(pal.ink, 0.07).name(QColor::HexArgb), alpha(pal.accent, 0.15).name(QColor::HexArgb),
             alpha(pal.accent, 0.6).name(QColor::HexArgb))
        + (character ? QStringLiteral(
            "QCheckBox#sheetChk { font-size: 11.5px; spacing: 7px; }"
            "QToolButton#sheetType { border-radius: 11px; padding: 0 7px; }"
            "QPushButton#sheetBtn, QPushButton#sheetPri { font-size: 11.5px; }"
            "QPushButton#sheetBtn { padding: 0 11px; } QPushButton#sheetPri { padding: 0 14px; }"
            "QLabel#sheetPickText { font-size: 11.5px; } QLabel#sheetRoleName { font-size: 12px; }")
                     : QString()));
    updatePreview();
}

void ElementCreateDialog::showEvent(QShowEvent* e)
{
    SheetDialog::showEvent(e);
    m_titleEdit->setFocus();
    if (!m_titleEdit->text().isEmpty()) m_titleEdit->selectAll();
}

void ElementCreateDialog::setInitial(const QString& title, const QString& role, const QString& imageDataUrl,
                                     bool narrator, const QString& trackMode, const QStringList& aliases)
{
    m_editMode = true;
    m_titleEdit->setText(title);
    if (m_aliasesEdit) m_aliasesEdit->setText(aliases.join(QStringLiteral(", ")));
    if (m_rolePick && !role.trimmed().isEmpty()) {
        QString found;
        for (int i = 0; i < kRoleCount; ++i)
            if (role.compare(QLatin1String(kRoles[i].id), Qt::CaseInsensitive) == 0) found = QString::fromLatin1(kRoles[i].id);
        if (!found.isEmpty()) setRole(found);
        else {
            // papel com nome próprio: "Outro…" com o texto dele
            QSignalBlocker b(m_roleCustom);
            m_roleCustom->setText(role);
            setRole(QStringLiteral("OUTRO"));
        }
    }
    if (m_narratorCheck) m_narratorCheck->setChecked(narrator);
    if (m_trackGroup) {
        const int t = trackMode == QLatin1String("on") ? 1 : trackMode == QLatin1String("off") ? 2 : 0;
        m_trackGroup->button(t)->setChecked(true);
    }
    m_imageDataUrl = imageDataUrl;
    updatePreview();
    if (m_okBtn) m_okBtn->setText(tr("Salvar"));
    setEnterHint(tr("salva"));
    const bool character = m_elementType == QStringLiteral("character");
    setEyebrow(character ? tr("Editar personagem")
               : m_elementType == QStringLiteral("setting") ? tr("Editar cenário")
               : m_elementType == QStringLiteral("object") ? tr("Editar objeto") : tr("Editar documento"));
    if (m_poster) m_poster->setEyebrow(tr("Editar personagem"));
    adjustSize();
}

void ElementCreateDialog::updatePreview()
{
    if (m_poster) {
        m_poster->setPhoto(pixmapFromDataUrl(m_imageDataUrl));
        refreshRoleTag();
        return;
    }
    if (!m_photoBtn) return;
    const QPixmap pm = pixmapFromDataUrl(m_imageDataUrl);
    // com foto, sem a borda tracejada do "+ foto"
    if (m_photoBtn->property("hasPhoto").toBool() != !pm.isNull()) {
        m_photoBtn->setProperty("hasPhoto", !pm.isNull());
        m_photoBtn->style()->unpolish(m_photoBtn);
        m_photoBtn->style()->polish(m_photoBtn);
    }
    if (pm.isNull()) {
        m_photoBtn->setIcon(QIcon());
        m_photoBtn->setText(tr("+ foto"));
        if (m_removePhoto) m_removePhoto->hide();
        refreshRoleTag();
        return;
    }
    // foto cortada no formato do botão (quadrado de cantos arredondados)
    const qreal dpr = devicePixelRatioF();
    const int W = m_photoBtn->width(), H = m_photoBtn->height();
    QPixmap out(QSize(W, H) * dpr);
    out.setDevicePixelRatio(dpr);
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    QPainterPath clip;
    clip.addRoundedRect(QRectF(0, 0, W, H), 10, 10);
    p.setClipPath(clip);
    const QPixmap sc = pm.scaled(QSize(W, H) * dpr, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    p.drawPixmap(QRectF(0, 0, W, H), sc,
                 QRectF((sc.width() - W * dpr) / 2, (sc.height() - H * dpr) / 2, W * dpr, H * dpr));
    p.end();
    m_photoBtn->setText(QString());
    m_photoBtn->setIcon(QIcon(out));
    m_photoBtn->setIconSize(QSize(W, H));
    if (m_removePhoto) m_removePhoto->show();
    refreshRoleTag();
}

void ElementCreateDialog::pickImage()
{
    const QString dataUrl = ImageCropDialog::pickAndCropImage(this);
    if (dataUrl.isEmpty()) return;
    m_imageDataUrl = dataUrl;
    updatePreview();
}

void ElementCreateDialog::updatePageTypeUi()
{
    if (!m_templateBox) return;
    m_templateBox->setVisible(createAsSheet() && !m_sheetTemplates.isEmpty());
    adjustSize();
}

void ElementCreateDialog::refreshRoleTag()
{
    if (!m_roleTag) return;
    QString label;
    if (m_roleValue == QLatin1String("OUTRO")) label = m_roleCustom ? m_roleCustom->text().trimmed() : QString();
    else for (const RoleOpt& r : kRoles) if (!m_roleValue.isEmpty() && m_roleValue == QLatin1String(r.id)) label = tr(r.label);
    m_roleTag->setText(label);
    m_roleTag->setVisible(!label.isEmpty());
    if (m_poster) m_poster->relayout();
}

void ElementCreateDialog::setRole(const QString& id)
{
    m_roleValue = id;
    const Palette pal = Palette::current();
    QString html;
    if (id.isEmpty()) {
        html = QStringLiteral("<span style='color:%1'>%2</span>").arg(pal.dim.name(), tr("Escolher papel (opcional)"));
    } else if (id == QLatin1String("OUTRO")) {
        const QString t = m_roleCustom ? m_roleCustom->text().trimmed() : QString();
        html = QStringLiteral("<span style='color:%1'>%2</span>").arg(pal.bright.name(),
                   (t.isEmpty() ? tr("Outro…") : t).toHtmlEscaped());
    } else {
        for (const RoleOpt& r : kRoles)
            if (id == QLatin1String(r.id))
                html = QStringLiteral("<span style='color:%1'>%2</span>&nbsp;&nbsp;<span style='color:%3'>· %4</span>")
                           .arg(pal.bright.name(), tr(r.label).toHtmlEscaped(), pal.dim.name(), tr(r.what).toHtmlEscaped());
    }
    if (m_rolePickText) m_rolePickText->setText(html);
    refreshRoleTag();
    if (m_roleCustom) m_roleCustom->setVisible(id == QLatin1String("OUTRO"));
    if (m_roleList)
        for (QPushButton* cell : m_roleList->findChildren<QPushButton*>(QStringLiteral("sheetRoleCell"))) {
            const bool on = cell->property("roleId").toString() == id;
            if (cell->property("sel").toBool() != on) {
                cell->setProperty("sel", on);
                cell->style()->unpolish(cell); cell->style()->polish(cell);
            }
        }
}

bool ElementCreateDialog::createAsSheet() const
{
    return m_pageGroup && m_pageGroup->checkedId() == 1;
}

QString ElementCreateDialog::selectedTemplateId() const
{
    if (!createAsSheet() || !m_templateGroup) return QString();
    const int i = m_templateGroup->checkedId();
    return i > 0 && i <= m_sheetTemplates.size() ? m_sheetTemplates[i - 1].id : QString();
}

QString ElementCreateDialog::title() const
{
    return m_titleEdit ? m_titleEdit->text().trimmed() : QString();
}

QString ElementCreateDialog::role() const
{
    if (m_roleValue == QLatin1String("OUTRO"))
        return m_roleCustom ? m_roleCustom->text().trimmed().toUpper() : QString();
    return m_roleValue;
}

bool ElementCreateDialog::narrator() const
{
    return m_narratorCheck ? m_narratorCheck->isChecked() : false;
}

QString ElementCreateDialog::trackMode() const
{
    if (!m_trackGroup) return QString();
    const int t = m_trackGroup->checkedId();
    return t == 1 ? QStringLiteral("on") : t == 2 ? QStringLiteral("off") : QString();
}

QStringList ElementCreateDialog::aliases() const
{
    if (!m_aliasesEdit) return QStringList();
    QStringList out;
    const QStringList parts = m_aliasesEdit->text().split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString& p : parts) {
        const QString t = p.trimmed();
        if (!t.isEmpty()) out.append(t);
    }
    return out;
}
