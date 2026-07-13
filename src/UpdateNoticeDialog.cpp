#include "UpdateNoticeDialog.h"

#include "Theme.h"

#include <QCloseEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
constexpr const char* kReleaseUrl =
    "https://github.com/Pedrovskigg/mira-writing-2-releases/releases/latest";
}

UpdateNoticeDialog::UpdateNoticeDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowFlags((windowFlags() | Qt::CustomizeWindowHint | Qt::WindowTitleHint)
                   & ~Qt::WindowCloseButtonHint & ~Qt::WindowContextHelpButtonHint);
    setModal(true);
    setFixedWidth(440);
    buildUi();
}

void UpdateNoticeDialog::buildUi()
{
    setWindowTitle(tr("O Mira Writing está de nome novo!"));
    setStyleSheet(QStringLiteral(R"(
        QDialog { background: %1; color: %2; }
        QLabel { color: %2; }
        QLabel#noticeTitle { color: %3; font-size: 17px; font-weight: 700; }
        QLabel#noticeLink a { color: %3; font-weight: 600; }
        QPushButton#okBtn {
            background: %3; color: %4; border: none;
            border-radius: 6px; padding: 8px 22px; font-weight: 600; font-size: 13px;
        }
        QPushButton#okBtn:hover { background: %5; }
    )").arg(Theme::panelBackground(), Theme::textPrimary(), Theme::accentDefault(),
            Theme::textBright(), Theme::borderStrong()));

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(26, 24, 26, 22);
    lay->setSpacing(14);

    auto* title = new QLabel(QStringLiteral("👋 ") + tr("O Mira Writing está de nome novo!"), this);
    title->setObjectName(QStringLiteral("noticeTitle"));
    title->setWordWrap(true);
    lay->addWidget(title);

    auto* bodyLbl = new QLabel(tr(
        "Olá! Estamos trocando de identidade — nome novo e repositório novo a "
        "caminho. Por causa disso, esta versão atual vai ficar paradinha por "
        "um tempo, sem novas atualizações.\n\n"
        "Mas já está resolvido: a versão com o nome novo já pode ser baixada "
        "aqui:"), this);
    bodyLbl->setWordWrap(true);
    lay->addWidget(bodyLbl);

    auto* linkLbl = new QLabel(this);
    linkLbl->setObjectName(QStringLiteral("noticeLink"));
    linkLbl->setText(QStringLiteral("<a href=\"%1\">%2</a>")
        .arg(QString::fromUtf8(kReleaseUrl), tr("Baixar a nova versão")));
    linkLbl->setTextFormat(Qt::RichText);
    linkLbl->setTextInteractionFlags(Qt::TextBrowserInteraction);
    linkLbl->setOpenExternalLinks(true);
    linkLbl->setCursor(Qt::PointingHandCursor);
    lay->addWidget(linkLbl);

    auto* footer = new QLabel(tr(
        "Seus projetos e arquivos salvos continuam 100% intactos — isso aqui "
        "é só uma troca de nome, não um sumiço. Obrigado pela paciência!"), this);
    footer->setWordWrap(true);
    lay->addWidget(footer);

    auto* okBtn = new QPushButton(tr("Entendi"), this);
    okBtn->setObjectName(QStringLiteral("okBtn"));
    okBtn->setCursor(Qt::PointingHandCursor);
    connect(okBtn, &QPushButton::clicked, this, [this]() {
        m_acknowledged = true;
        accept();
    });

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    btnRow->addWidget(okBtn);
    lay->addLayout(btnRow);
}

void UpdateNoticeDialog::closeEvent(QCloseEvent* event)
{
    if (!m_acknowledged) {
        event->ignore();
        return;
    }
    QDialog::closeEvent(event);
}

void UpdateNoticeDialog::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        event->ignore();
        return;
    }
    QDialog::keyPressEvent(event);
}
