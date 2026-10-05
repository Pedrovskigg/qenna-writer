#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QEasingCurve>
#include <QElapsedTimer>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QLinearGradient>
#include <QLocale>
#include <QPainter>
#include <QPixmap>
#include <QRegularExpression>
#include <QScreen>
#include <QSet>
#include <QSettings>
#include "WhatsNewDialog.h"
#include <QSplashScreen>
#include <QStringList>
#include <QThread>
#include <QTranslator>
#include <QVector>
#include <QtMath>

#include "CrashLogger.h"
#include "MainWindow.h"
#include "QuickCover.h"
#include "WindowChrome.h"
#include "Theme.h"

namespace {

// Subfamílias de tamanho óptico (ex: "Bodoni Moda 11pt", "Bodoni Moda 11pt Black")
// são registradas como famílias separadas pelo Qt mas são inúteis no picker —
// a família base ("Bodoni Moda") já cobre todos os pesos via variable font.
const QRegularExpression kOpticalSizeRe(QStringLiteral("\\d+pt"));

// Splash animada: o Q surge sozinho no centro, desliza pro lugar dele e as
// outras letras saem de trás dele, uma depois da outra, até formar QENNA; no
// fim, um reflexo de luz atravessa o logo. kSplashMinMs é o tempo mínimo total na tela: o
// Qenna carrega rápido, e sem ele o logo montado apareceria por um piscar
// antes da janela assumir.
constexpr int kSplashQFadeMs = 285;      // Q aparecendo sozinho
constexpr int kSplashQAloneMs = 250;     // Q parado sozinho antes de abrir caminho
constexpr int kSplashSlideStartMs = kSplashQFadeMs + kSplashQAloneMs;
constexpr int kSplashQSlideMs = 1550;    // Q indo do centro pra ponta esquerda, sem pressa
constexpr int kSplashSlideMs = 780;      // viagem de cada letra
constexpr int kSplashStaggerMs = 65;     // atraso entre uma letra e a próxima
constexpr int kSplashShineGapMs = 60;    // respiro entre a última letra e o brilho
constexpr int kSplashShineMs = 450;      // reflexo de luz atravessando o logo
constexpr int kSplashMinMs = 2675;
constexpr int kSplashFrameMs = 16;
constexpr qreal kSplashLetterDpr = 2.0;  // as letras vêm em 2x: nítidas em tela com zoom
constexpr qreal kSplashGap = 5.5;        // espaço entre letras, em px lógicos

struct SplashLetters {
    QVector<QPixmap> pixmaps; // Q, E, N, N, A
    QVector<qreal> finalX;
    QSizeF size;              // logo montado, em px lógicos
};

SplashLetters loadSplashLetters()
{
    SplashLetters s;
    qreal x = 0;
    qreal h = 0;
    for (const char *name : {"q", "e", "n1", "n2", "a"}) {
        QPixmap pm(QStringLiteral(":/app/splash-letters/%1.png").arg(QLatin1String(name)));
        pm.setDevicePixelRatio(kSplashLetterDpr);
        const QSizeF logical = pm.deviceIndependentSize();
        if (!s.pixmaps.isEmpty()) x += kSplashGap;
        s.pixmaps.append(pm);
        s.finalX.append(x);
        x += logical.width();
        h = qMax(h, logical.height());
    }
    s.size = QSizeF(x, h);
    return s;
}

int splashLettersDoneMs(const SplashLetters &s)
{
    return qMax(kSplashSlideStartMs + kSplashQSlideMs,
                kSplashSlideStartMs + kSplashStaggerMs * int(s.pixmaps.size() - 2) + kSplashSlideMs);
}

int splashAnimationMs(const SplashLetters &s)
{
    return splashLettersDoneMs(s) + kSplashShineGapMs + kSplashShineMs;
}

QPixmap splashFrame(const SplashLetters &s, int elapsedMs, qreal dpr)
{
    QPixmap frame(qCeil(s.size.width() * dpr), qCeil(s.size.height() * dpr));
    frame.setDevicePixelRatio(dpr);
    frame.fill(Qt::transparent);
    if (s.pixmaps.isEmpty()) return frame;

    const QEasingCurve slide(QEasingCurve::OutCubic);
    auto progress = [elapsedMs](int startMs, int durationMs) {
        return qBound(qreal(0), (elapsedMs - startMs) / qreal(durationMs), qreal(1));
    };

    // Q: parte do centro do logo e termina na ponta esquerda.
    const QSizeF qSize = s.pixmaps.first().deviceIndependentSize();
    const qreal qStartX = (s.size.width() - qSize.width()) / 2;
    auto qXAt = [&](int ms) {
        const qreal p = qBound(qreal(0), (ms - kSplashSlideStartMs) / qreal(kSplashQSlideMs), qreal(1));
        return qStartX + (s.finalX.first() - qStartX) * slide.valueForProgress(p);
    };

    QPainter painter(&frame);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    // De trás pra frente: o A é a camada mais funda e o Q fica por cima de
    // todas, então cada letra sai literalmente de trás das anteriores.
    for (int i = int(s.pixmaps.size()) - 1; i >= 1; --i) {
        const int startMs = kSplashSlideStartMs + kSplashStaggerMs * (i - 1);
        const qreal p = progress(startMs, kSplashSlideMs);
        if (p <= 0) continue;
        const qreal width = s.pixmaps.at(i).deviceIndependentSize().width();
        // Nasce escondida atrás de onde o Q está no instante em que sai.
        const qreal fromX = qXAt(startMs) + (qSize.width() - width) / 2;
        const qreal x = fromX + (s.finalX.at(i) - fromX) * slide.valueForProgress(p);
        // Acende no começo da viagem: atrás do Q ela ainda espiaria pelos
        // cantos arredondados e pelo miolo dele.
        painter.setOpacity(QEasingCurve(QEasingCurve::InOutQuad).valueForProgress(qMin(qreal(1), p / qreal(0.3))));
        painter.drawPixmap(QPointF(x, 0), s.pixmaps.at(i));
    }

    painter.setOpacity(QEasingCurve(QEasingCurve::OutQuad).valueForProgress(progress(0, kSplashQFadeMs)));
    painter.drawPixmap(QPointF(qXAt(elapsedMs), 0), s.pixmaps.first());

    // Logo montado: um reflexo de luz inclinado atravessa da esquerda pra
    // direita, como num letreiro. SourceAtop pinta só onde já tem letra, sem
    // mexer na transparência em volta. Acaba antes do carregamento, que
    // congela o quadro: o que fica parado na tela é o logo limpo.
    const qreal shine = progress(splashLettersDoneMs(s) + kSplashShineGapMs, kSplashShineMs);
    if (shine > 0 && shine < 1) {
        const qreal band = 150;
        const qreal cx = -band + (s.size.width() + 2 * band)
                       * QEasingCurve(QEasingCurve::InOutSine).valueForProgress(shine);
        const QPointF center(cx, s.size.height() / 2);
        const QPointF along = QPointF(1, 0.36) * (band / qSqrt(1 + 0.36 * 0.36));
        QLinearGradient light(center - along, center + along);
        light.setColorAt(0, QColor(255, 248, 230, 0));
        light.setColorAt(0.5, QColor(255, 248, 230, 125));
        light.setColorAt(1, QColor(255, 248, 230, 0));
        painter.setOpacity(1);
        painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
        painter.fillRect(QRectF(0, 0, s.size.width(), s.size.height()), light);
    }
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

// Todo tooltip do Qt (QTipLabel, objectName "qtooltip_label") ganha um estilo
// PRÓPRIO com as cores do tema na hora de aparecer. Sem isso, um painel com
// "background: transparent" sem seletor (tem dezenas no app) cascateia até a
// etiqueta do tooltip, que fica transparente: o Windows pinta preto por baixo
// e o texto sai na cor do tema (ilegível em tema claro). Estilo do próprio
// widget ganha de qualquer estilo herdado.
class TooltipThemeFilter : public QObject {
public:
    using QObject::QObject;
protected:
    bool eventFilter(QObject* o, QEvent* e) override {
        if (e->type() != QEvent::Show || o->objectName() != QLatin1String("qtooltip_label")) return false;
        auto* w = qobject_cast<QWidget*>(o);
        if (!w) return false;
        QColor bg = Theme::toColor(Theme::panelBackground());
        bg.setAlpha(255);
        const QString qss = Theme::qss(QStringLiteral(
            "* { background-color: %1; color: %2; border: 1px solid %3;"
            " border-radius: @radius-control; padding: 4px 8px; }"))
            .arg(bg.name(), Theme::textPrimary(), Theme::panelBorder());
        if (w->styleSheet() != qss) w->setStyleSheet(qss);
        return false;
    }
};

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
    // Tem que ser a primeira leitura do QSettings depois de nomear o app: logo
    // abaixo o idioma detectado já é gravado, e o registro deixaria de parecer
    // vazio numa instalação nova.
    WhatsNewDialog::markFreshInstall(QSettings().allKeys().isEmpty());
    QApplication::setWindowIcon(QIcon(":/app/mira.png"));

    CrashLogger::install();

    const SplashLetters splashLetters = loadSplashLetters();
    const qreal splashDpr = QGuiApplication::primaryScreen()
        ? QGuiApplication::primaryScreen()->devicePixelRatio() : qreal(1);

    QSplashScreen splash(splashFrame(splashLetters, 0, splashDpr));
    splash.setAttribute(Qt::WA_TranslucentBackground);
    splash.setWindowFlag(Qt::FramelessWindowHint);
    splash.show();
    app.processEvents();

    QElapsedTimer splashClock;
    splashClock.start();

    // As letras se montam aqui, antes do carregamento, porque daqui até
    // splash.finish() tudo é síncrono: não há event loop, então um QTimer
    // nunca dispararia.
    {
        const int totalMs = splashAnimationMs(splashLetters);
        forever {
            const int elapsed = int(qMin<qint64>(splashClock.elapsed(), totalMs));
            splash.setPixmap(splashFrame(splashLetters, elapsed, splashDpr));
            QApplication::processEvents();
            if (elapsed >= totalMs)
                break;
            QThread::msleep(kSplashFrameMs);
        }
    }

    // Stylesheet global vive em Theme::globalStyleSheet() — derivada do tema
    // corrente. MainWindow::onThemeChanged() reaplica em troca de tema.
    app.setStyleSheet(Theme::globalStyleSheet());
    Theme::applyToolTipPalette();
    app.installEventFilter(new TooltipThemeFilter(&app));

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
        // Textos do próprio Qt (Fechar, Cancelar, menu de copiar/colar). O
        // inglês não tem arquivo e segue o padrão do Qt.
        static QTranslator qtBaseTranslator;
        if (!resolvedLang.isEmpty() && qtBaseTranslator.load(QStringLiteral(":/i18n/qtbase_") + resolvedLang))
            QApplication::installTranslator(&qtBaseTranslator);
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
    QuickCover::setBundledFamilies(customFontFamilies);
    // toda janela com barra do sistema (Lousa, Timeline, avisos…) no tema, não só o editor
    WindowChrome::install();

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
