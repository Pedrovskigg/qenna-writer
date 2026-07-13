#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFontDatabase>
#include <QIcon>
#include <QLocale>
#include <QPixmap>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QSplashScreen>
#include <QStringList>
#include <QTranslator>

#include "CrashLogger.h"
#include "MainWindow.h"
#include "Theme.h"
#include "UpdateNoticeDialog.h"

namespace {

// Subfamílias de tamanho óptico (ex: "Bodoni Moda 11pt", "Bodoni Moda 11pt Black")
// são registradas como famílias separadas pelo Qt mas são inúteis no picker —
// a família base ("Bodoni Moda") já cobre todos os pesos via variable font.
const QRegularExpression kOpticalSizeRe(QStringLiteral("\\d+pt"));

QStringList registerCustomFonts()
{
    QString fontsDir = QCoreApplication::applicationDirPath() + QStringLiteral("/fonts");
    if (!QDir(fontsDir).exists()) {
        fontsDir = QString::fromUtf8(DEV_ASSETS_DIR) + QStringLiteral("/fonts");
    }
    if (!QDir(fontsDir).exists()) {
        return {};
    }

    QSet<QString> families;
    QDirIterator it(fontsDir,
                    {QStringLiteral("*.ttf"), QStringLiteral("*.otf")},
                    QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const int id = QFontDatabase::addApplicationFont(path);
        if (id != -1) {
            for (const QString &family : QFontDatabase::applicationFontFamilies(id)) {
                if (kOpticalSizeRe.match(family).hasMatch()) continue;
                families.insert(family);
            }
        }
    }
    QStringList result = families.values();
    result.sort(Qt::CaseInsensitive);
    return result;
}

}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QApplication::setApplicationName("Mira Writing");
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    QApplication::setOrganizationName("Mira Writing");
    QApplication::setWindowIcon(QIcon(":/app/mira.png"));

    CrashLogger::install();

    QSplashScreen splash(QPixmap(":/app/splash.png"));
    splash.setAttribute(Qt::WA_TranslucentBackground);
    splash.setWindowFlag(Qt::FramelessWindowHint);
    splash.show();
    app.processEvents();

    // Stylesheet global vive em Theme::globalStyleSheet() — derivada do tema
    // corrente. MainWindow::onThemeChanged() reaplica em troca de tema.
    app.setStyleSheet(Theme::globalStyleSheet());

    QTranslator translator;
    {
        // Preferência do usuário tem prioridade; cai no locale do sistema como fallback.
        QSettings qs;
        const QString prefLang = qs.value(QStringLiteral("app/language")).toString();
        bool loaded = false;
        if (!prefLang.isEmpty()) {
            loaded = translator.load(QStringLiteral(":/i18n/mira_") + prefLang);
        }
        if (!loaded) {
            for (const QString &locale : QLocale::system().uiLanguages()) {
                if (translator.load(QStringLiteral(":/i18n/mira_") + QLocale(locale).name())) {
                    loaded = true;
                    break;
                }
            }
        }
        if (loaded) QApplication::installTranslator(&translator);
    }

    // Aviso temporário (troca de nome/repo) — bloqueia o startup até o
    // usuário confirmar que leu. Remover depois que a migração pra Qiyva
    // estiver concluída e essa mensagem deixar de fazer sentido.
    {
        splash.close();
        UpdateNoticeDialog notice;
        notice.exec();
    }

    const QStringList customFontFamilies = registerCustomFonts();

    // Mescla fontes customizadas com fontes do sistema (Calibri, Arial, Times
    // New Roman, Cambria, etc.), aplicando o mesmo filtro de subfamílias ópticas.
    QSet<QString> allFamilies(customFontFamilies.begin(), customFontFamilies.end());
    for (const QString &f : QFontDatabase::families()) {
        if (!kOpticalSizeRe.match(f).hasMatch())
            allFamilies.insert(f);
    }
    QStringList allFontFamilies = allFamilies.values();
    allFontFamilies.sort(Qt::CaseInsensitive);

    MainWindow window;
    window.setAvailableFontFamilies(allFontFamilies);

    // Só revela a janela se já tem projeto carregado (autoOpen). Sem projeto,
    // o construtor agenda a abertura do Main Menu — e a MainWindow ganha
    // show() depois, quando o usuário escolher/criar um projeto.
    if (window.hasProjectLoaded()) {
        window.show();
        splash.finish(&window);
    } else {
        splash.close();
    }

    return app.exec();
}
