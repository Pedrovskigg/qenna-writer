#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QEasingCurve>
#include <QElapsedTimer>
#include <QFontDatabase>
#include <QIcon>
#include <QImage>
#include <QLocale>
#include <QPainter>
#include <QPixmap>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QSplashScreen>
#include <QStringList>
#include <QThread>
#include <QTranslator>

#include "CrashLogger.h"
#include "MainWindow.h"
#include "Theme.h"

namespace {

// Subfamílias de tamanho óptico (ex: "Bodoni Moda 11pt", "Bodoni Moda 11pt Black")
// são registradas como famílias separadas pelo Qt mas são inúteis no picker —
// a família base ("Bodoni Moda") já cobre todos os pesos via variable font.
const QRegularExpression kOpticalSizeRe(QStringLiteral("\\d+pt"));

// Splash: duração do fade preto e branco -> cor, e tempo mínimo total que a
// tela fica visível. O Qenna carrega rápido, então sem o mínimo a arte
// colorida apareceria por um piscar — o fade terminaria e a janela já estaria
// pronta pra assumir.
constexpr int kSplashFadeMs = 900;
constexpr int kSplashMinMs = 2200;
constexpr int kSplashFrameMs = 16;

// Dessatura preservando o canal alpha. Format_Grayscale8 seria mais curto mas
// descarta a transparência, e a arte do splash é recortada — viraria um
// retângulo opaco na tela.
QImage desaturated(const QImage &source)
{
    QImage img = source.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < img.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            const int g = qGray(line[x]);
            line[x] = qRgba(g, g, g, qAlpha(line[x]));
        }
    }
    return img;
}

// Mistura a versão colorida sobre a cinza. SourceAtop em vez do SourceOver
// padrão porque ele preserva o alpha do destino: compondo por cima na marra,
// as bordas anti-aliased das letras somariam opacidade e ganhariam halo no
// meio da transição.
QPixmap splashFrame(const QPixmap &gray, const QPixmap &color, qreal progress)
{
    QPixmap frame(color.size());
    frame.setDevicePixelRatio(color.devicePixelRatio());
    frame.fill(Qt::transparent);

    QPainter painter(&frame);
    painter.drawPixmap(0, 0, gray);
    painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
    painter.setOpacity(progress);
    painter.drawPixmap(0, 0, color);
    return frame;
}

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
    // MOTOR DE FONTES — precisa ser escolhido ANTES do QApplication.
    //
    // O padrão no Windows é o DirectWrite, e ele NÃO entrega ao gerador de PDF
    // o arquivo da fonte quando ela foi carregada pelo app (as 123 famílias que
    // vêm na pasta fonts/). Sem o arquivo, o Qt desiste de embutir e desenha
    // cada letra como contorno vetorial: o PDF de um manuscrito passa de ~120 KB
    // para ~30 MB, e o texto deixa de ser texto — não dá pra selecionar, buscar
    // nem copiar, e nenhuma editora aceita isso.
    //
    // Medido no mesmo capítulo, mesma fonte (Alegreya, do bundle):
    //   directwrite  30.713 KB   fonte NÃO embutida
    //   gdi             135 KB   fonte embutida
    //   freetype        124 KB   fonte embutida
    //
    // FreeType resolve porque guarda o caminho do arquivo da fonte, que é
    // exatamente o que o gerador de PDF procura. Fontes do sistema (Georgia,
    // Times) já funcionavam nos três — o problema era só com as do bundle.
    //
    // Se algum dia isso for revertido, o sintoma volta como "exportação de PDF
    // gigante" ou "margem enorme no PDF", que não parecem problema de fonte.
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "windows:fontengine=freetype");

    QApplication app(argc, argv);

    // Modo de teste: QENNA_FRESH_PROFILE=1 faz o QSettings (registro do
    // Windows) apontar pra uma chave separada, sempre vazia — simula "abrir o
    // app pela primeira vez" (sem tema/chave de API/última resolução salvos)
    // sem tocar no perfil de verdade. Útil pra testar os defaults que reagem à
    // resolução da tela (ver ScreenDefaults) em várias resoluções seguidas:
    // repita o launch com a variável setada, nada fica gravado no perfil real.
    const bool freshProfile = qEnvironmentVariableIsSet("QENNA_FRESH_PROFILE");
    const QString appName = freshProfile
        ? QStringLiteral("Qenna Writer (Fresh Test)")
        : QStringLiteral("Qenna Writer");

    if (!freshProfile) {
        migrateSettingsFromMira();
        migrateSettingsFromQiyva();
    }

    QApplication::setApplicationName(appName);
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    QApplication::setOrganizationName(appName);
    QApplication::setWindowIcon(QIcon(":/app/mira.png"));

    CrashLogger::install();

    const QPixmap splashColor(QStringLiteral(":/app/splash-3.png"));
    const QPixmap splashGray = QPixmap::fromImage(desaturated(splashColor.toImage()));

    QSplashScreen splash(splashGray);
    splash.setAttribute(Qt::WA_TranslucentBackground);
    splash.setWindowFlag(Qt::FramelessWindowHint);
    splash.show();
    app.processEvents();

    QElapsedTimer splashClock;
    splashClock.start();

    // A arte abre em preto e branco e ganha cor — as cinco paletas do logo
    // acendendo. Roda aqui, antes do carregamento, porque daqui até
    // splash.finish() tudo é síncrono: não há event loop, então um QTimer
    // nunca dispararia.
    {
        const QEasingCurve curve(QEasingCurve::InOutQuad);
        forever {
            const qreal linear = qMin(qreal(1), splashClock.elapsed() / qreal(kSplashFadeMs));
            splash.setPixmap(splashFrame(splashGray, splashColor, curve.valueForProgress(linear)));
            QApplication::processEvents();
            if (linear >= qreal(1))
                break;
            QThread::msleep(kSplashFrameMs);
        }
    }

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
        QString resolvedLang;
        if (!prefLang.isEmpty()) {
            loaded = translator.load(QStringLiteral(":/i18n/qenna_") + prefLang);
            if (loaded) resolvedLang = prefLang;
        }
        if (!loaded) {
            for (const QString &locale : QLocale::system().uiLanguages()) {
                const QString code = QLocale(locale).name();
                if (translator.load(QStringLiteral(":/i18n/qenna_") + code)) {
                    loaded = true;
                    resolvedLang = code;
                    break;
                }
            }
        }
        // Locale do sistema sem tradução disponível: cai pro inglês (não pro
        // pt-BR do código-fonte) — só quem já é falante de português tende a
        // ter o Windows configurado em pt-BR.
        if (!loaded) {
            loaded = translator.load(QStringLiteral(":/i18n/qenna_en"));
            if (loaded) resolvedLang = QStringLiteral("en");
        }
        if (loaded) QApplication::installTranslator(&translator);
        // Primeira execução (sem preferência salva ainda): grava o idioma
        // detectado em app/language, senão o combo do Main Menu e os pontos
        // que leem essa chave direto (fora do QTranslator — GeoData,
        // WordCountPanel, etc.) ficariam presos assumindo pt-BR por default
        // enquanto a UI já mostra outro idioma.
        if (prefLang.isEmpty() && !resolvedLang.isEmpty()) {
            qs.setValue(QStringLiteral("app/language"), resolvedLang);
        }
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

    // Segura o splash até o tempo mínimo. Não é atraso fixo: se o carregamento
    // já passou disso — projeto grande, disco lento — não espera nada.
    while (splashClock.elapsed() < kSplashMinMs) {
        QApplication::processEvents();
        QThread::msleep(kSplashFrameMs);
    }

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
