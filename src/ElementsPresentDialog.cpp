#include "ElementsPresentDialog.h"

#include "ElementsStore.h"
#include "Theme.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

ElementsPresentDialog::ElementsPresentDialog(ElementsStore* store, const QString& docKey, QWidget* parent)
    : QDialog(parent)
    , m_store(store)
    , m_docKey(docKey)
{
    setWindowTitle(tr("Elementos presentes…"));
    setModal(true);
    setAttribute(Qt::WA_DeleteOnClose);
    setFixedWidth(320);
    buildUi();
}

void ElementsPresentDialog::buildUi()
{
    setStyleSheet(QStringLiteral(R"(
        QDialog { background: %1; color: %2; }
        QLabel#groupLabel {
            color: %3;
            font-size: 10px;
            font-weight: 700;
            letter-spacing: 0.5px;
            padding-top: 6px;
        }
        QCheckBox { color: %2; padding: 3px 0; }
        QPushButton#okBtn {
            background: %4; color: %5; border: none;
            border-radius: 6px; padding: 6px 14px; font-weight: 600;
        }
        QPushButton#okBtn:hover { background: %6; }
        QPushButton#cancelBtn {
            background: transparent; color: %3;
            border: 1px solid %7; border-radius: 6px; padding: 6px 14px;
        }
        QPushButton#cancelBtn:hover { background: %8; }
    )").arg(Theme::panelBackground(), Theme::textPrimary(), Theme::textMuted(),
            Theme::accentDefault(), Theme::textBright(), Theme::borderStrong(),
            Theme::panelBorder(), Theme::hoverOverlay()));

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(16, 16, 16, 16);
    outer->setSpacing(10);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setFixedHeight(320);
    scroll->setStyleSheet(QStringLiteral("background: transparent;"));

    auto* inner = new QWidget(scroll);
    inner->setStyleSheet(QStringLiteral("background: transparent;"));
    auto* lay = new QVBoxLayout(inner);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(2);

    m_checks.clear();
    bool anyElement = false;
    if (m_store) {
        const QList<Element> all = m_store->elements();
        for (const ElementType& type : m_store->elementTypes()) {
            QList<Element> ofType;
            for (const Element& e : all) {
                if (e.type == type.id) ofType.append(e);
            }
            if (ofType.isEmpty()) continue;
            anyElement = true;

            auto* groupLbl = new QLabel(type.label.toUpper(), inner);
            groupLbl->setObjectName(QStringLiteral("groupLabel"));
            lay->addWidget(groupLbl);

            for (const Element& e : ofType) {
                auto* check = new QCheckBox(e.name.isEmpty() ? tr("(sem nome)") : e.name, inner);
                check->setChecked(m_store->hasDocElement(m_docKey, e.id));
                lay->addWidget(check);
                m_checks.append(qMakePair(e.id, check));
            }
        }
    }

    if (!anyElement) {
        auto* emptyLbl = new QLabel(tr("Nenhum elemento no projeto ainda."), inner);
        emptyLbl->setWordWrap(true);
        emptyLbl->setStyleSheet(QStringLiteral("color: %1; font-style: italic;").arg(Theme::textMuted()));
        lay->addWidget(emptyLbl);
    }

    lay->addStretch();
    scroll->setWidget(inner);
    outer->addWidget(scroll);

    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);
    auto* cancelBtn = new QPushButton(tr("Cancelar"), this);
    cancelBtn->setObjectName(QStringLiteral("cancelBtn"));
    auto* okBtn = new QPushButton(tr("Salvar"), this);
    okBtn->setObjectName(QStringLiteral("okBtn"));
    okBtn->setDefault(true);
    btnRow->addStretch();
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(okBtn);
    outer->addLayout(btnRow);

    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
}

void ElementsPresentDialog::accept()
{
    if (m_store) {
        for (const auto& pair : m_checks) {
            const QString& elementId = pair.first;
            const bool checked = pair.second->isChecked();
            const bool wasPresent = m_store->hasDocElement(m_docKey, elementId);
            if (checked && !wasPresent) m_store->addDocElement(m_docKey, elementId);
            else if (!checked && wasPresent) m_store->removeDocElement(m_docKey, elementId);
        }
    }
    QDialog::accept();
}
