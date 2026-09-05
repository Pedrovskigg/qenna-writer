#include "TrashDialog.h"

#include "Theme.h"
#include "TrashService.h"

#include <QDateTime>
#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

QString formatDeletedAt(qint64 ms)
{
    const QDateTime dt = QDateTime::fromMSecsSinceEpoch(ms);
    const qint64 days = dt.daysTo(QDateTime::currentDateTime());
    if (days <= 0) return QObject::tr("excluído hoje");
    if (days == 1) return QObject::tr("excluído ontem");
    return QObject::tr("excluído há %1 dias").arg(days);
}

} // namespace

TrashDialog::TrashDialog(QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("trashDialog"));
    setWindowTitle(tr("Lixeira"));
    setModal(true);
    setMinimumSize(460, 360);
    resize(520, 420);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 18);
    root->setSpacing(14);

    auto* title = new QLabel(tr("Projetos excluídos"), this);
    title->setObjectName(QStringLiteral("trashTitle"));
    root->addWidget(title);

    auto* subtitle = new QLabel(
        tr("Projetos apagados ficam aqui até você restaurar ou apagar em definitivo. "
           "Nada mais é excluído direto pelo app."), this);
    subtitle->setObjectName(QStringLiteral("trashSubtitle"));
    subtitle->setWordWrap(true);
    root->addWidget(subtitle);

    m_emptyLabel = new QLabel(tr("A lixeira está vazia."), this);
    m_emptyLabel->setObjectName(QStringLiteral("trashEmpty"));
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    root->addWidget(m_emptyLabel);

    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("trashScroll"));
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));

    m_listHolder = new QWidget(scroll);
    m_listHolder->setObjectName(QStringLiteral("trashListHolder"));
    m_listLayout = new QVBoxLayout(m_listHolder);
    m_listLayout->setContentsMargins(0, 0, 0, 0);
    m_listLayout->setSpacing(8);
    m_listLayout->addStretch(1);
    scroll->setWidget(m_listHolder);
    root->addWidget(scroll, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    root->addWidget(buttons);

    applyTheme();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &TrashDialog::applyTheme);

    rebuildList();
}

void TrashDialog::rebuildList()
{
    // Limpa as linhas antigas (mantém o stretch final no fim do layout).
    while (m_listLayout->count() > 1) {
        QLayoutItem* item = m_listLayout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    const QVector<TrashedProjectEntry> entries = TrashService::listTrashedProjects();
    m_emptyLabel->setVisible(entries.isEmpty());
    m_listHolder->setVisible(!entries.isEmpty());

    for (const TrashedProjectEntry& entry : entries) {
        auto* row = new QFrame(m_listHolder);
        row->setObjectName(QStringLiteral("trashRow"));
        auto* rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(12, 10, 12, 10);
        rowLay->setSpacing(10);

        auto* infoCol = new QVBoxLayout();
        infoCol->setSpacing(2);
        auto* nameLbl = new QLabel(entry.displayName.isEmpty()
                                        ? tr("(projeto sem nome)")
                                        : entry.displayName, row);
        nameLbl->setObjectName(QStringLiteral("trashRowName"));
        auto* metaLbl = new QLabel(
            tr("%1 — %2").arg(formatDeletedAt(entry.deletedAtMs), entry.originalPath), row);
        metaLbl->setObjectName(QStringLiteral("trashRowMeta"));
        metaLbl->setWordWrap(true);
        infoCol->addWidget(nameLbl);
        infoCol->addWidget(metaLbl);
        rowLay->addLayout(infoCol, 1);

        auto* restoreBtn = new QPushButton(tr("Restaurar"), row);
        restoreBtn->setObjectName(QStringLiteral("trashRestoreBtn"));
        restoreBtn->setCursor(Qt::PointingHandCursor);
        const QString id = entry.id;
        connect(restoreBtn, &QPushButton::clicked, this, [this, id]() {
            QString err;
            QString restoredPath;
            if (!TrashService::restoreProject(id, &restoredPath, &err)) {
                QMessageBox::warning(this, tr("Erro ao restaurar"), err);
                return;
            }
            QMessageBox::information(this, tr("Projeto restaurado"),
                tr("O projeto foi restaurado em:\n%1\n\nAbra-o pela tela inicial "
                   "(\"Carregar pasta\") se ele não aparecer sozinho nos recentes.")
                    .arg(restoredPath));
            rebuildList();
        });
        rowLay->addWidget(restoreBtn);

        auto* purgeBtn = new QPushButton(tr("Apagar definitivo"), row);
        purgeBtn->setObjectName(QStringLiteral("trashPurgeBtn"));
        purgeBtn->setCursor(Qt::PointingHandCursor);
        const QString name = entry.displayName;
        connect(purgeBtn, &QPushButton::clicked, this, [this, id, name]() {
            const auto ans = QMessageBox::warning(this, tr("Apagar em definitivo"),
                tr("Apagar \"%1\" em definitivo? Isso não pode ser desfeito.").arg(name),
                QMessageBox::Cancel | QMessageBox::Yes, QMessageBox::Cancel);
            if (ans != QMessageBox::Yes) return;
            QString err;
            if (!TrashService::purgeProject(id, &err)) {
                QMessageBox::warning(this, tr("Erro ao apagar"), err);
                return;
            }
            rebuildList();
        });
        rowLay->addWidget(purgeBtn);

        m_listLayout->insertWidget(m_listLayout->count() - 1, row);
    }
}

void TrashDialog::applyTheme()
{
    setStyleSheet(QStringLiteral(R"(
        #trashDialog {
            background: %1;
        }
        #trashTitle {
            color: %2;
            font-size: 16px;
            font-weight: 600;
        }
        #trashSubtitle {
            color: %3;
            font-size: 12px;
        }
        #trashEmpty {
            color: %3;
            font-size: 12px;
            padding: 24px 0;
        }
        #trashScroll {
            background: transparent;
            border: none;
        }
        #trashListHolder {
            background: transparent;
        }
        QFrame#trashRow {
            background: %4;
            border: 1px solid %5;
            border-radius: 8px;
        }
        #trashRowName {
            color: %2;
            font-size: 13px;
            font-weight: 600;
        }
        #trashRowMeta {
            color: %3;
            font-size: 11px;
        }
        QPushButton#trashRestoreBtn {
            background: %6;
            color: %2;
            border: 1px solid %7;
            border-radius: 6px;
            padding: 5px 12px;
            font-size: 12px;
        }
        QPushButton#trashRestoreBtn:hover {
            background: %8;
        }
        QPushButton#trashPurgeBtn {
            background: transparent;
            color: %9;
            border: 1px solid %10;
            border-radius: 6px;
            padding: 5px 12px;
            font-size: 12px;
        }
        QPushButton#trashPurgeBtn:hover {
            background: %10;
            color: %2;
        }
        #trashDialog QDialogButtonBox QPushButton {
            background: %4;
            color: %2;
            border: 1px solid %5;
            padding: 6px 18px;
            border-radius: 6px;
            font-size: 12px;
        }
        #trashDialog QDialogButtonBox QPushButton:hover {
            background: %8;
        }
    )").arg(
        Theme::panelBackground(),      // 1
        Theme::textPrimary(),          // 2
        Theme::textMuted(),            // 3
        Theme::appBackground(),        // 4
        Theme::panelBorder(),          // 5
        Theme::accentSuccessSoft(),    // 6
        Theme::accentSuccessBorderSoft(), // 7
        Theme::hoverOverlay(),         // 8
        Theme::accentDanger(),         // 9
        Theme::accentDangerBorderSoft() // 10
    ));
}
