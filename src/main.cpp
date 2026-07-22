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

// Quotes.cpp cacheia o ciclo embaralhado como texto bruto (não traduzido) em
// QSettings, pra persistir entre sessões — ver Quotes::next(). Esse cache
// carrega o nome do app dentro do próprio texto de vários quotes, então
// arrastar essas duas chaves de um rebrand pro outro deixaria quotes com o
// nome antigo grudado no texto até o cache expirar sozinho. As duas
// migrações abaixo pulam essas chaves de propósito, forçando o ciclo a ser
// regerado do zero (com o texto atual) assim que uma migração acontece.
const QSet<QString> kQuoteCacheKeysToSkip = {
    QStringLiteral("quotes/cycle"),
    QStringLiteral("quotes/pointer"),
};

// Migração única do rebranding Mira Writing → Qiyva Writer: copia todas as
// chaves do registro antigo (HKCU\Software\Mira Writing\Mira Writing) pra
// chave nova, preservando tema, idioma, progresso do contador de palavras,
// projetos recentes, etc. Idempotente — roda uma vez só, marcada por flag
// na chave nova. Não mexe em logs de crash (dado de baixo valor).
void migrateSettingsFromMira()
{
    QSettings newSettings(QStringLiteral("Qiyva Writer"), QStringLiteral("Qiyva Writer"));
    if (newSettings.value(QStringLiteral("migratedFromMira"), false).toBool())
        return;

    QSettings oldSettings(QStringLiteral("Mira Writing"), QStringLiteral("Mira Writing"));
    const QStringList keys = oldSettings.allKeys();
    for (const QString &key : keys) {
        if (kQuoteCacheKeysToSkip.contains(key)) continue;
        newSettings.setValue(key, oldSettings.value(key));
    }
    newSettings.setValue(QStringLiteral("migratedFromMira"), true);
}

// Migração única do rebranding Qiyva Writer → Qenna Writer: mesmo esquema da
// migração acima, mas partindo da chave "Qiyva Writer" (que só existiu por
// pouco tempo em produção). Roda depois da migração de Mira, então cobre os
// dois saltos possíveis (Mira→Qenna direto, ou Mira→Qiyva→Qenna).
void migrateSettingsFromQiyva()
{
    QSettings newSettings(QStringLiteral("Qenna Writer"), QStringLiteral("Qenna Writer"));
    if (newSettings.value(QStringLiteral("migratedFromQiyva"), false).toBool())
        return;

    QSettings oldSettings(QStringLiteral("Qiyva Writer"), QStringLiteral("Qiyva Writer"));
    const QStringList keys = oldSettings.allKeys();
    for (const QString &key : keys) {
        if (kQuoteCacheKeysToSkip.contains(key)) continue;
        newSettings.setValue(key, oldSettings.value(key));
    }
    newSettings.setValue(QStringLiteral("migratedFromQiyva"), true);
}

}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    migrateSettingsFromMira();
    migrateSettingsFromQiyva();

    QApplication::setApplicationName("Qenna Writer");
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    QApplication::setOrganizationName("Qenna Writer");
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
    Theme::applyToolTipPalette();

    QTranslator translator;
    {
        // Preferência do usuário tem prioridade; cai no locale do sistema como fallback.
        QSettings qs;
        const QString prefLang = qs.value(QStringLiteral("app/language")).toString();
        bool loaded = false;
        if (!prefLang.isEmpty()) {
            loaded = translator.load(QStringLiteral(":/i18n/qenna_") + prefLang);
        }
        if (!loaded) {
            for (const QString &locale : QLocale::system().uiLanguages()) {
                if (translator.load(QStringLiteral(":/i18n/qenna_") + QLocale(locale).name())) {
                    loaded = true;
                    break;
                }
            }
        }
        if (loaded) QApplication::installTranslator(&translator);
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
