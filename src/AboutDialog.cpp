#include "AboutDialog.h"

#include "Theme.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("aboutDialog"));
    setWindowTitle(tr("Sobre o Qiyva Writer"));
    setModal(true);
    setFixedWidth(500);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(32, 28, 32, 24);
    root->setSpacing(0);

    auto* mainText = new QLabel(this);
    mainText->setObjectName(QStringLiteral("aboutMainText"));
    mainText->setWordWrap(true);
    mainText->setText(
        tr("Qiyva Writer é um software de escrita criativa, desenvolvido por um amante "
           "da escrita para outros escritores.\n\n"
           "Cada função, recurso, opção e funcionalidade foi pensada para tornar o "
           "projeto de escrita melhor.\n\n"
           "Ele é um programa gratuito e a ideia é que sempre seja. O pagamento pelo "
           "uso é escrever boas histórias.\n\n"
           "Aproveite."));
    root->addWidget(mainText);

    root->addSpacing(20);

    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setObjectName(QStringLiteral("aboutSeparator"));
    root->addWidget(sep);

    root->addSpacing(14);

    // Email: clique copia pro clipboard
    auto* emailRow = new QHBoxLayout();
    emailRow->setContentsMargins(0, 0, 0, 0);
    emailRow->setSpacing(0);

    m_emailBtn = new QPushButton(QStringLiteral("mirawritingeditor@gmail.com"), this);
    m_emailBtn->setObjectName(QStringLiteral("aboutInlineBtn"));
    m_emailBtn->setCursor(Qt::PointingHandCursor);
    m_emailBtn->setToolTip(tr("Clique para copiar"));
    connect(m_emailBtn, &QPushButton::clicked, this, [this]() {
        QGuiApplication::clipboard()->setText(QStringLiteral("mirawritingeditor@gmail.com"));
        m_emailBtn->setText(tr("Copiado!"));
        QTimer::singleShot(2000, m_emailBtn, [this]() {
            m_emailBtn->setText(QStringLiteral("mirawritingeditor@gmail.com"));
        });
    });
    emailRow->addWidget(m_emailBtn);

    auto* emailDesc = new QLabel(tr("  |  para bugs, sugestões e contato geral."), this);
    emailDesc->setObjectName(QStringLiteral("aboutFooter"));
    emailRow->addWidget(emailDesc);
    emailRow->addStretch();
    root->addLayout(emailRow);

    root->addSpacing(6);

    // Github: clique abre no browser
    auto* githubRow = new QHBoxLayout();
    githubRow->setContentsMargins(0, 0, 0, 0);
    githubRow->setSpacing(0);

    auto* githubPrefix = new QLabel(QStringLiteral("Github:  "), this);
    githubPrefix->setObjectName(QStringLiteral("aboutFooter"));
    githubRow->addWidget(githubPrefix);

    auto* githubBtn = new QPushButton(QStringLiteral("https://github.com/Pedrovskigg"), this);
    githubBtn->setObjectName(QStringLiteral("aboutInlineBtn"));
    githubBtn->setCursor(Qt::PointingHandCursor);
    githubBtn->setToolTip(tr("Abrir no navegador"));
    connect(githubBtn, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/Pedrovskigg")));
    });
    githubRow->addWidget(githubBtn);
    githubRow->addStretch();
    root->addLayout(githubRow);

    root->addSpacing(6);

    // Código-fonte: clique abre no browser
    auto* sourceRow = new QHBoxLayout();
    sourceRow->setContentsMargins(0, 0, 0, 0);
    sourceRow->setSpacing(0);

    auto* sourcePrefix = new QLabel(tr("Código-fonte:  "), this);
    sourcePrefix->setObjectName(QStringLiteral("aboutFooter"));
    sourceRow->addWidget(sourcePrefix);

    auto* sourceBtn = new QPushButton(
        QStringLiteral("https://github.com/Pedrovskigg/qiyva-writer"), this);
    sourceBtn->setObjectName(QStringLiteral("aboutInlineBtn"));
    sourceBtn->setCursor(Qt::PointingHandCursor);
    sourceBtn->setToolTip(tr("Abrir no navegador"));
    connect(sourceBtn, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(
            QUrl(QStringLiteral("https://github.com/Pedrovskigg/qiyva-writer")));
    });
    sourceRow->addWidget(sourceBtn);
    sourceRow->addStretch();
    root->addLayout(sourceRow);

    root->addSpacing(20);

    auto* authorLabel = new QLabel(
        QStringLiteral("Qiyva Writer is a solo project, created by:\nP.H. Lobato\n\n−2026"),
        this);
    authorLabel->setObjectName(QStringLiteral("aboutAuthor"));
    root->addWidget(authorLabel);

    root->addSpacing(10);

    auto* versionLabel = new QLabel(
        tr("Current version: %1").arg(QCoreApplication::applicationVersion()),
        this);
    versionLabel->setObjectName(QStringLiteral("aboutVersion"));
    root->addWidget(versionLabel);

    root->addSpacing(20);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    root->addWidget(buttons);

    applyTheme();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &AboutDialog::applyTheme);
}

void AboutDialog::applyTheme()
{
    setStyleSheet(QStringLiteral(R"(
        #aboutDialog {
            background: %1;
        }
        #aboutMainText {
            color: %2;
            font-size: 13px;
        }
        #aboutSeparator {
            color: %3;
        }
        #aboutFooter {
            color: %4;
            font-size: 11px;
        }
        QPushButton#aboutInlineBtn {
            background: transparent;
            color: %4;
            border: none;
            padding: 0;
            font-size: 11px;
            text-decoration: underline;
            text-align: left;
        }
        QPushButton#aboutInlineBtn:hover {
            color: %2;
        }
        #aboutAuthor {
            color: %2;
            font-size: 12px;
        }
        #aboutVersion {
            color: %4;
            font-size: 11px;
            font-style: italic;
        }
        #aboutDialog QPushButton {
            background: %5;
            color: %2;
            border: 1px solid %3;
            padding: 6px 18px;
            border-radius: 6px;
            font-size: 12px;
        }
        #aboutDialog QPushButton:hover {
            background: %6;
            color: %7;
        }
    )").arg(
        Theme::panelBackground(),  // 1
        Theme::textPrimary(),      // 2
        Theme::panelBorder(),      // 3
        Theme::textMuted(),        // 4
        Theme::panelBackground(),  // 5
        Theme::hoverOverlay(),     // 6
        Theme::textBright()        // 7
    ));
}
