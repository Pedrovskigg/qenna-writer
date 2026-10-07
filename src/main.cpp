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
// fim, um reflexo de luz atravessa o logo. No caminho, cada letra vai trocando
// de mundo em cortes secos (os mundos do Q do menu), como a vinheta da Marvel,
// até assentar na arte dela; e o logo, que nasce em preto e branco, vai
// ganhando cor. kSplashMinMs é o tempo mínimo total na tela: o
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
constexpr int kSplashColorEndMs = 1850;  // cor cheia um pouco antes do logo terminar de montar
constexpr int kSplashSettleMs = 1640;    // o Q para de piscar e assenta na arte dele...
constexpr int kSplashSettleStepMs = 95;  // ...e cada letra seguinte um pouco depois, em onda
constexpr int kSplashMinMs = 2675;
constexpr int kSplashFrameMs = 16;
constexpr qreal kSplashLetterDpr = 2.0;  // as letras vêm em 2x: nítidas em tela com zoom
constexpr qreal kSplashGap = 5.5;        // espaço entre letras, em px lógicos

struct SplashLetter {
    QPixmap color;           // arte da letra, 2x
    QPixmap gray;
    QSizeF size;             // px lógicos
    QStringList worlds;      // mundos que piscam, já na ordem de exibição
    QVector<qreal> cuts;     // instante de cada corte
    int settleMs = 0;
    int cachedWorld = -1;    // só o mundo da vez fica decodificado
    QPixmap worldColor;
    QPixmap worldGray;
};

struct SplashLetters {
    QVector<SplashLetter> letters; // Q, E, N, N, A
    QVector<qreal> finalX;
    QSizeF size;                   // logo montado, em px lógicos
};

// Preto e branco preservando o alpha (as letras são recortadas). Os pesos são
// os do saturate() do CSS: a prévia aprovada foi feita no navegador.
QPixmap grayscaled(const QImage &source)
{
    QImage img = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < img.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            const QRgb p = line[x];
            const int g = (54 * qRed(p) + 183 * qGreen(p) + 19 * qBlue(p)) >> 8;
            line[x] = qRgba(g, g, g, qAlpha(p));
        }
    }
    return QPixmap::fromImage(img);
}

// Cortes secos que vão desacelerando: rápidos no começo, cada mundo durando
// mais conforme a letra chega perto de assentar.
QVector<qreal> splashCuts(int startMs, int settleMs)
{
    QVector<qreal> cuts;
    for (qreal t = startMs; t < settleMs;) {
        cuts.append(t);
        const qreal p = (t - startMs) / qreal(settleMs - startMs);
        t += 55 + 150 * p * p;
    }
    return cuts;
}

SplashLetters loadSplashLetters()
{
    SplashLetters s;
    qreal x = 0;
    qreal h = 0;
    int i = 0;
    for (const char *name : {"q", "e", "n1", "n2", "a"}) {
        SplashLetter l;
        const QImage art(QStringLiteral(":/app/splash-letters/%1.png").arg(QLatin1String(name)));
        l.color = QPixmap::fromImage(art);
        l.color.setDevicePixelRatio(kSplashLetterDpr);
        l.gray = grayscaled(art);
        l.gray.setDevicePixelRatio(kSplashLetterDpr);
        l.size = l.color.deviceIndependentSize();

        const QString worldsDir = QStringLiteral(":/app/splash-worlds/%1/").arg(QLatin1String(name));
        for (const QString &file : QDir(worldsDir).entryList(QDir::Files, QDir::Name))
            l.worlds.append(worldsDir + file);
        // O Q já nasce piscando; as outras começam assim que saem de trás dele.
        const int startMs = i == 0 ? 0 : kSplashSlideStartMs + kSplashStaggerMs * (i - 1);
        l.settleMs = kSplashSettleMs + kSplashSettleStepMs * i;
        l.cuts = splashCuts(startMs, l.settleMs);

        if (!s.letters.isEmpty()) x += kSplashGap;
        s.finalX.append(x);
        x += l.size.width();
        h = qMax(h, l.size.height());
        s.letters.append(l);
        ++i;
    }
    s.size = QSizeF(x, h);
    return s;
}

int splashLettersDoneMs(const SplashLetters &s)
{
    return qMax(kSplashSlideStartMs + kSplashQSlideMs,
                kSplashSlideStartMs + kSplashStaggerMs * int(s.letters.size() - 2) + kSplashSlideMs);
}

int splashAnimationMs(const SplashLetters &s)
{
    return splashLettersDoneMs(s) + kSplashShineGapMs + kSplashShineMs;
}

// Pinta a letra no instante: um mundo enquanto pisca, a arte dela depois de
// assentar, misturando a versão cinza com a colorida na proporção da cor (a
// mesma conta do saturate() do CSS). A mistura é feita num pixmap à parte pra
// que a opacidade de entrada da letra caia sobre o resultado: compondo direto
// no quadro, as bordas anti-aliased somariam opacidade e ganhariam halo.
// Cada mundo só é decodificado quando o corte dele chega: aparece por ~100 ms,
// e carregar os 76 antes atrasaria a abertura no PC fraco.
void paintSplashLetter(QPainter &painter, SplashLetter &l, int index, int elapsedMs,
                       const QPointF &pos, qreal color, qreal dpr)
{
    const QPixmap *colorArt = &l.color;
    const QPixmap *grayArt = &l.gray;
    if (!l.worlds.isEmpty() && !l.cuts.isEmpty()
        && elapsedMs >= l.cuts.first() && elapsedMs < l.settleMs) {
        int n = 0;
        while (n + 1 < l.cuts.size() && l.cuts.at(n + 1) <= elapsedMs) ++n;
        const int world = (n + index * 3) % int(l.worlds.size());
        if (world != l.cachedWorld) {
            l.worldColor = QPixmap(l.worlds.at(world));
            l.worldGray = QPixmap();
            l.cachedWorld = world;
        }
        if (color < 1 && l.worldGray.isNull())
            l.worldGray = grayscaled(l.worldColor.toImage());
        colorArt = &l.worldColor;
        grayArt = &l.worldGray;
    }

    const QRectF target(pos, l.size);
    if (color >= 1 || color <= 0) {
        const QPixmap &art = color >= 1 ? *colorArt : *grayArt;
        painter.drawPixmap(target, art, QRectF(art.rect()));
        return;
    }
    QPixmap mixed(qCeil(l.size.width() * dpr), qCeil(l.size.height() * dpr));
    mixed.setDevicePixelRatio(dpr);
    mixed.fill(Qt::transparent);
    {
        QPainter p(&mixed);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        const QRectF local(QPointF(0, 0), l.size);
        p.drawPixmap(local, *grayArt, QRectF(grayArt->rect()));
        p.setCompositionMode(QPainter::CompositionMode_SourceAtop);
        p.setOpacity(color);
        p.drawPixmap(local, *colorArt, QRectF(colorArt->rect()));
    }
    painter.drawPixmap(QRectF(pos, mixed.deviceIndependentSize()), mixed, QRectF(mixed.rect()));
}

QPixmap splashFrame(SplashLetters &s, int elapsedMs, qreal dpr)
{
    QPixmap frame(qCeil(s.size.width() * dpr), qCeil(s.size.height() * dpr));
    frame.setDevicePixelRatio(dpr);
    frame.fill(Qt::transparent);
    if (s.letters.isEmpty()) return frame;

    const QEasingCurve slide(QEasingCurve::OutCubic);
    auto progress = [elapsedMs](int startMs, int durationMs) {
        return qBound(qreal(0), (elapsedMs - startMs) / qreal(durationMs), qreal(1));
    };
    // A cor começa a entrar assim que o Q termina de surgir e chega mais
    // depressa no começo: o cinza é só a largada.
    const qreal color = qSin(M_PI / 2 * progress(kSplashQFadeMs, kSplashColorEndMs - kSplashQFadeMs));

    // Q: parte do centro do logo e termina na ponta esquerda.
    const QSizeF qSize = s.letters.first().size;
    const qreal qStartX = (s.size.width() - qSize.width()) / 2;
    auto qXAt = [&](int ms) {
        const qreal p = qBound(qreal(0), (ms - kSplashSlideStartMs) / qreal(kSplashQSlideMs), qreal(1));
        return qStartX + (s.finalX.first() - qStartX) * slide.valueForProgress(p);
    };

    QPainter painter(&frame);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    // De trás pra frente: o A é a camada mais funda e o Q fica por cima de
    // todas, então cada letra sai literalmente de trás das anteriores.
    for (int i = int(s.letters.size()) - 1; i >= 1; --i) {
        const int startMs = kSplashSlideStartMs + kSplashStaggerMs * (i - 1);
        const qreal p = progress(startMs, kSplashSlideMs);
        if (p <= 0) continue;
        const qreal width = s.letters.at(i).size.width();
        // Nasce escondida atrás de onde o Q está no instante em que sai.
        const qreal fromX = qXAt(startMs) + (qSize.width() - width) / 2;
        const qreal x = fromX + (s.finalX.at(i) - fromX) * slide.valueForProgress(p);
        // Acende no começo da viagem: atrás do Q ela ainda espiaria pelos
        // cantos arredondados e pelo miolo dele.
        painter.setOpacity(QEasingCurve(QEasingCurve::InOutQuad).valueForProgress(qMin(qreal(1), p / qreal(0.3))));
        paintSplashLetter(painter, s.letters[i], i, elapsedMs, QPointF(x, 0), color, dpr);
    }

    painter.setOpacity(QEasingCurve(QEasingCurve::OutQuad).valueForProgress(progress(0, kSplashQFadeMs)));
    paintSplashLetter(painter, s.letters[0], 0, elapsedMs, QPointF(qXAt(elapsedMs), 0), color, dpr);

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

// Tarja preta atrás do logo, do Q ao A. Não existe no começo: nasce como uma
// linha fina no centro, se estica até as pontas e depois abre pra cima e pra
// baixo, devagar, inteira no último quadro da animação. O logo é
// montado no pixmap dele e só depois vai pra cima da tarja: o reflexo usa
// SourceAtop e, pintado direto sobre o preto, cobriria a tarja inteira.
constexpr qreal kSplashBandPadX = 24;  // folga da tarja além do Q e do A
constexpr qreal kSplashBandPadY = 48;  // folga da tarja acima e abaixo do logo
constexpr qreal kSplashBandLine = 2;   // espessura da linha em que ela nasce
constexpr qreal kSplashBandSpread = 0.4; // fração do tempo da linha se esticando
constexpr int kSplashBandInMs = 1300;  // a linha aparece aqui
// Tamanho da splash na tela. Tudo acima é medido no tamanho cheio das letras;
// a escala só entra aqui, com o logo já desenhado na resolução final.
constexpr qreal kSplashScale = 0.65;

QPixmap splashCanvasFrame(SplashLetters &s, int elapsedMs, qreal dpr)
{
    const QSizeF canvas(s.size.width() + 2 * kSplashBandPadX, s.size.height() + 2 * kSplashBandPadY);
    QPixmap frame(qCeil(canvas.width() * kSplashScale * dpr), qCeil(canvas.height() * kSplashScale * dpr));
    frame.setDevicePixelRatio(dpr);
    frame.fill(Qt::transparent);

    const QPixmap logo = splashFrame(s, elapsedMs, dpr * kSplashScale);
    QPainter painter(&frame);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.scale(kSplashScale, kSplashScale);
    const int endMs = splashAnimationMs(s);
    const qreal band = qBound(qreal(0), (elapsedMs - kSplashBandInMs) / qreal(endMs - kSplashBandInMs), qreal(1));
    if (band > 0) {
        const qreal wide = QEasingCurve(QEasingCurve::InOutSine)
            .valueForProgress(qMin(qreal(1), band / kSplashBandSpread));
        const qreal tall = QEasingCurve(QEasingCurve::InOutSine)
            .valueForProgress(qMax(qreal(0), (band - kSplashBandSpread) / (1 - kSplashBandSpread)));
        const qreal w = canvas.width() * wide;
        const qreal h = kSplashBandLine + (canvas.height() - kSplashBandLine) * tall;
        painter.fillRect(QRectF((canvas.width() - w) / 2, (canvas.height() - h) / 2, w, h), Qt::black);
    }
    const QPointF at(kSplashBandPadX, kSplashBandPadY);
    painter.drawPixmap(QRectF(at, logo.deviceIndependentSize()), logo, QRectF(logo.rect()));
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

    const qreal splashDpr = QGuiApplication::primaryScreen()
        ? QGuiApplication::primaryScreen()->devicePixelRatio() : qreal(1);

    QSplashScreen splash;
    splash.setAttribute(Qt::WA_TranslucentBackground);
    splash.setWindowFlag(Qt::FramelessWindowHint);
    QElapsedTimer splashClock;

    // As letras se montam aqui, antes do carregamento, porque daqui até
    // splash.finish() tudo é síncrono: não há event loop, então um QTimer
    // nunca dispararia. As artes só vivem durante a animação: depois o splash
    // fica com o último quadro e a memória volta.
    {
        SplashLetters splashLetters = loadSplashLetters();
        splash.setPixmap(splashCanvasFrame(splashLetters, 0, splashDpr));
        splash.show();
        app.processEvents();
        splashClock.start();

        const int totalMs = splashAnimationMs(splashLetters);
        forever {
            const int elapsed = int(qMin<qint64>(splashClock.elapsed(), totalMs));
            splash.setPixmap(splashCanvasFrame(splashLetters, elapsed, splashDpr));
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
