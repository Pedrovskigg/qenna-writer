#include "WhatsNewDialog.h"

#include "Theme.h"
#include "UpdateChecker.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QFile>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace {
const char kLastSeenKey[] = "app/whatsNewLastSeenVersion";
constexpr int kLogoWidth = 440;
}

WhatsNewDialog::WhatsNewDialog(QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("whatsNewDialog"));
    setWindowTitle(tr("Novidades do Qenna Writer"));
    setAttribute(Qt::WA_StyledBackground, true);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(32, 24, 32, 20);
    root->setSpacing(0);

    // Logo do splash atual. Escalado já na densidade da tela: um pixmap
    // reduzido em 1x ficaria borrado em monitor com escala de 150%.
    m_logo = new QLabel(this);
    m_logo->setAlignment(Qt::AlignCenter);
    {
        const qreal dpr = devicePixelRatioF();
        QPixmap logo(QStringLiteral(":/app/splash-4.png"));
        if (!logo.isNull()) {
            logo = logo.scaledToWidth(qRound(kLogoWidth * dpr), Qt::SmoothTransformation);
            logo.setDevicePixelRatio(dpr);
            m_logo->setPixmap(logo);
        }
    }
    root->addWidget(m_logo);
    root->addSpacing(12);

    m_title = new QLabel(tr("Novidades da versão %1").arg(QCoreApplication::applicationVersion()), this);
    m_title->setObjectName(QStringLiteral("whatsNewTitle"));
    m_title->setAlignment(Qt::AlignCenter);
    root->addWidget(m_title);
    root->addSpacing(14);

    m_body = new QTextBrowser(this);
    m_body->setObjectName(QStringLiteral("whatsNewBody"));
    m_body->setOpenExternalLinks(true);
    m_body->setFrameShape(QFrame::NoFrame);
    m_body->document()->setDocumentMargin(4);
    m_body->setMarkdown(loadNotes());
    root->addWidget(m_body, 1);
    root->addSpacing(14);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    if (auto* close = buttons->button(QDialogButtonBox::Close)) close->setText(tr("Fechar"));
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    root->addWidget(buttons);

    // Alto o bastante pra ler com conforto, sem passar da tela em notebook.
    int h = 760;
    if (const QScreen* s = screen()) h = qMin(h, s->availableGeometry().height() - 80);
    resize(640, h);

    applyTheme();
    connect(Theme::Manager::instance(), &Theme::Manager::themeChanged,
            this, &WhatsNewDialog::applyTheme);
}

void WhatsNewDialog::markFreshInstall(bool settingsWereEmpty)
{
    if (settingsWereEmpty) markSeen();
}

bool WhatsNewDialog::shouldShow()
{
    // Sem a chave e sem ser instalação nova = veio de uma versão anterior a
    // esta janela existir. Isso também é atualização, então mostra.
    const QString lastSeen = QSettings().value(QLatin1String(kLastSeenKey)).toString();
    if (lastSeen.isEmpty()) return true;
    return UpdateChecker::compareVersions(QCoreApplication::applicationVersion(), lastSeen) > 0;
}

void WhatsNewDialog::markSeen()
{
    QSettings().setValue(QLatin1String(kLastSeenKey), QCoreApplication::applicationVersion());
}

QString WhatsNewDialog::loadNotes() const
{
    // Idioma do app ("pt_BR", "en", "it"...): tenta o código inteiro, depois só
    // a língua, e por fim o inglês — mesmo fallback do tradutor no main.cpp.
    const QString lang = QSettings().value(QStringLiteral("app/language")).toString();
    QStringList candidates;
    if (!lang.isEmpty()) {
        candidates << lang << lang.section(QLatin1Char('_'), 0, 0);
        if (lang.startsWith(QLatin1String("pt"))) candidates << QStringLiteral("pt_BR");
    }
    candidates << QStringLiteral("en");

    QString text;
    for (const QString& code : std::as_const(candidates)) {
        QFile f(QStringLiteral(":/whatsnew/whatsnew_%1.md").arg(code));
        if (f.open(QIODevice::ReadOnly)) {
            text = QString::fromUtf8(f.readAll());
            break;
        }
    }

    // O patch note é escrito como texto corrido: cada linha é uma linha. No
    // markdown, uma quebra simples vira espaço e junta tudo num parágrafo só —
    // então toda linha seguida de outra ganha quebra forçada (dois espaços).
    QStringList lines = text.split(QLatin1Char('\n'));
    for (int i = 0; i + 1 < lines.size(); ++i) {
        QString& line = lines[i];
        if (line.endsWith(QLatin1Char('\r'))) line.chop(1);
        const bool nextHasText = !lines.at(i + 1).trimmed().isEmpty();
        if (!line.trimmed().isEmpty() && nextHasText && !line.startsWith(QLatin1Char('#')))
            line += QStringLiteral("  ");
    }
    return lines.join(QLatin1Char('\n'));
}

void WhatsNewDialog::applyTheme()
{
    setStyleSheet(QStringLiteral(R"(
        #whatsNewDialog {
            background: %1;
        }
        #whatsNewTitle {
            color: %4;
            font-size: 12px;
            letter-spacing: 1px;
        }
        QTextBrowser#whatsNewBody {
            background: transparent;
            color: %2;
            font-size: 13px;
            selection-background-color: %6;
        }
        #whatsNewDialog QPushButton {
            background: %1;
            color: %2;
            border: 1px solid %3;
            padding: 6px 18px;
            border-radius: 6px;
            font-size: 12px;
        }
        #whatsNewDialog QPushButton:hover {
            background: %5;
            color: %7;
        }
    )").arg(
        Theme::panelBackground(),  // 1
        Theme::textPrimary(),      // 2
        Theme::panelBorder(),      // 3
        Theme::textMuted(),        // 4
        Theme::hoverOverlay(),     // 5
        Theme::accentDefault(),    // 6
        Theme::textBright()        // 7
    ));
    // O viewport do QTextBrowser não herda o transparent do QSS sozinho.
    m_body->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
}
