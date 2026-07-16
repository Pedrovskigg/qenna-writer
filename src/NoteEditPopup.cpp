#include "NoteEditPopup.h"

#include "Theme.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
const QStringList kPalette = {
    QStringLiteral("#FFD54F"), QStringLiteral("#FF8A80"),
    QStringLiteral("#80D8FF"), QStringLiteral("#A5D6A7"),
    QStringLiteral("#CE93D8"), QStringLiteral("#FFAB91"),
    QStringLiteral("#EF9A9A"), QStringLiteral("#80CBC4"),
};
constexpr int kSwatchSize = 22;

QString swatchStyle(const QString& color, bool selected)
{
    const QString border = selected
        ? QStringLiteral("2px solid %1").arg(Theme::textBright())
        : QStringLiteral("1px solid %1").arg(Theme::subtleBorder());
    return QStringLiteral(
        "QToolButton { background:%1; border:%2; border-radius:4px;"
        "  min-width:%3px; max-width:%3px; min-height:%3px; max-height:%3px; }"
        "QToolButton:hover { border:2px solid %4; }"
    ).arg(color, border, QString::number(kSwatchSize), Theme::textBright());
}
}

NoteEditPopup::NoteEditPopup(QWidget* parent)
    : QFrame(parent)
    , m_color(kPalette.first())
{
    setObjectName(QStringLiteral("noteEditPopup"));
    buildUi();
    applyTheme();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &NoteEditPopup::applyTheme);
    hide();
}

void NoteEditPopup::buildUi()
{
    setFixedWidth(360);
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 14, 16, 14);
    root->setSpacing(10);

    m_title = new QLabel(tr("Nova nota"), this);
    m_title->setObjectName(QStringLiteral("noteTitle"));
    root->addWidget(m_title);

    m_titleEdit = new QLineEdit(this);
    m_titleEdit->setObjectName(QStringLiteral("noteTitleEdit"));
    m_titleEdit->setPlaceholderText(tr("Título (opcional)"));
    root->addWidget(m_titleEdit);

    m_textEdit = new QTextEdit(this);
    m_textEdit->setObjectName(QStringLiteral("noteText"));
    m_textEdit->setPlaceholderText(tr("Escreva sua anotação…"));
    m_textEdit->setMinimumHeight(120);
    root->addWidget(m_textEdit);

    auto* palette = new QWidget(this);
    m_paletteRow = new QHBoxLayout(palette);
    m_paletteRow->setContentsMargins(0, 0, 0, 0);
    m_paletteRow->setSpacing(8);
    buildPalette();
    m_paletteRow->addStretch();
    root->addWidget(palette);

    auto* actions = new QHBoxLayout();
    actions->setContentsMargins(0, 0, 0, 0);
    actions->addStretch();
    m_cancelBtn = new QToolButton(this);
    m_cancelBtn->setObjectName(QStringLiteral("noteCancel"));
    m_cancelBtn->setText(tr("Cancelar"));
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(m_cancelBtn, &QToolButton::clicked, this, [this]() {
        hide();
        emit cancelled();
    });
    actions->addWidget(m_cancelBtn);

    m_saveBtn = new QToolButton(this);
    m_saveBtn->setObjectName(QStringLiteral("noteSave"));
    m_saveBtn->setText(tr("Salvar"));
    m_saveBtn->setCursor(Qt::PointingHandCursor);
    connect(m_saveBtn, &QToolButton::clicked, this, &NoteEditPopup::emitConfirm);
    actions->addWidget(m_saveBtn);

    root->addLayout(actions);
}

void NoteEditPopup::buildPalette()
{
    for (const QString& c : kPalette) {
        auto* sw = new QToolButton(this);
        sw->setCursor(Qt::PointingHandCursor);
        sw->setStyleSheet(swatchStyle(c, false));
        connect(sw, &QToolButton::clicked, this, [this, c]() {
            m_color = QColor(c);
            applySwatchSelection();
        });
        m_paletteRow->addWidget(sw);
        m_swatches.append(sw);
    }
}

void NoteEditPopup::applySwatchSelection()
{
    for (int i = 0; i < m_swatches.size(); ++i) {
        const QString c = kPalette.at(i);
        const bool selected = (QColor(c) == m_color);
        m_swatches[i]->setStyleSheet(swatchStyle(c, selected));
    }
}

void NoteEditPopup::openForCreate()
{
    m_title->setText(tr("Nova nota"));
    m_color = QColor(kPalette.first());
    m_titleEdit->clear();
    m_textEdit->clear();
    applySwatchSelection();
    centerInParent();
    show();
    raise();
    m_titleEdit->setFocus();
}

void NoteEditPopup::openForEdit(const QColor& color, const QString& title, const QString& text)
{
    m_title->setText(tr("Editar nota"));
    m_color = color.isValid() ? color : QColor(kPalette.first());
    m_titleEdit->setText(title);
    m_textEdit->setPlainText(text);
    applySwatchSelection();
    centerInParent();
    show();
    raise();
    m_textEdit->setFocus();
}

void NoteEditPopup::centerInParent()
{
    QWidget* p = parentWidget();
    if (!p) return;
    adjustSize();
    const int x = (p->width() - width()) / 2;
    const int y = (p->height() - height()) / 3; // um pouco acima do centro
    move(qMax(0, x), qMax(0, y));
}

void NoteEditPopup::emitConfirm()
{
    const QString title = m_titleEdit->text().trimmed();
    const QString text = m_textEdit->toPlainText().trimmed();
    if (title.isEmpty() && text.isEmpty()) {
        m_textEdit->setFocus();
        return; // nota totalmente vazia não faz sentido
    }
    hide();
    emit confirmed(m_color, title, text);
}

void NoteEditPopup::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        hide();
        emit cancelled();
        return;
    }
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        && (event->modifiers() & Qt::ControlModifier)) {
        emitConfirm();
        return;
    }
    QFrame::keyPressEvent(event);
}

void NoteEditPopup::applyTheme()
{
    setStyleSheet(QStringLiteral(R"(
        #noteEditPopup {
            background: %1;
            border: 1px solid %2;
            border-radius: 10px;
        }
        #noteTitle { color: %3; font-size: 15px; font-weight: 600; }
        #noteTitleEdit {
            background: %4;
            color: %3;
            border: 1px solid %5;
            border-radius: 8px;
            padding: 7px 8px;
            font-size: 13px;
        }
        #noteText {
            background: %4;
            color: %3;
            border: 1px solid %5;
            border-radius: 8px;
            padding: 8px;
            font-size: 13px;
        }
        #noteCancel {
            color: %6; background: transparent; border: none;
            border-radius: 6px; padding: 6px 14px; font-size: 13px;
        }
        #noteCancel:hover { background: %7; color: %3; }
        #noteSave {
            color: %3; background: %8; border: none;
            border-radius: 6px; padding: 6px 16px; font-size: 13px; font-weight: 600;
        }
        #noteSave:hover { background: %9; }
    )")
        .arg(Theme::panelBackground(), Theme::borderStrong(), Theme::textPrimary(),
             Theme::inputBackground(), Theme::subtleBorder(), Theme::textMuted(),
             Theme::hoverStrong(), Theme::accentDefault(), Theme::hoverStrong()));
    applySwatchSelection();
}
