#include "ThemePackage.h"

#include "ZipReader.h"
#include "ZipWriter.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUuid>

namespace {

const char* kThemeJsonName = "theme.json";

// Teto pro arquivo inteiro. Um tema é JSON miúdo mais uma imagem de fundo;
// 64 MB já é generoso e evita carregar um arquivo absurdo na memória só pra
// descobrir depois que não era tema.
constexpr qint64 kMaxPackageBytes = 64LL * 1024 * 1024;

// Extensões que aceitamos como imagem de fundo. Lista fechada: o campo vem de
// arquivo de terceiro, então não dá pra confiar no que ele diz ser.
bool isAllowedImageSuffix(const QString& suffix)
{
    static const QStringList allowed = {
        QStringLiteral("png"),  QStringLiteral("jpg"), QStringLiteral("jpeg"),
        QStringLiteral("webp"), QStringLiteral("bmp")
    };
    return allowed.contains(suffix.toLower());
}

// Compara "0.17.1" com "0.18.0" numericamente, campo a campo. Devolve <0, 0
// ou >0. Pedaço não numérico conta como 0 — é comparação de ordem, não de
// semântica completa.
int compareVersions(const QString& a, const QString& b)
{
    const QStringList pa = a.split(QLatin1Char('.'));
    const QStringList pb = b.split(QLatin1Char('.'));
    const int n = qMax(pa.size(), pb.size());
    for (int i = 0; i < n; ++i) {
        const int va = i < pa.size() ? pa.at(i).toInt() : 0;
        const int vb = i < pb.size() ? pb.at(i).toInt() : 0;
        if (va != vb) return va < vb ? -1 : 1;
    }
    return 0;
}

// Pasta onde as imagens de temas importados passam a morar. Fica na área de
// dados do app (não no projeto): tema é configuração global, não conteúdo de
// um manuscrito.
QString importedImagesDir()
{
    return QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                           + QStringLiteral("/theme-backgrounds"));
}

} // namespace

namespace ThemePackage {

QString extension() { return QStringLiteral("qtheme"); }

QStringList licenseCodes()
{
    return {
        QStringLiteral("free-with-credit"),
        QStringLiteral("free"),
        QStringLiteral("personal-only"),
        QStringLiteral("all-rights-reserved"),
    };
}

QString licenseDisplayName(const QString& code)
{
    if (code == QLatin1String("free-with-credit"))    return QCoreApplication::translate("ThemePackage", "Uso livre, com crédito ao autor");
    if (code == QLatin1String("free"))                return QCoreApplication::translate("ThemePackage", "Uso livre, sem restrições");
    if (code == QLatin1String("personal-only"))       return QCoreApplication::translate("ThemePackage", "Somente uso pessoal");
    if (code == QLatin1String("all-rights-reserved")) return QCoreApplication::translate("ThemePackage", "Todos os direitos reservados");
    return code; // licença escrita pelo próprio autor
}

QString fileFilter()
{
    return QCoreApplication::translate("ThemePackage", "Tema do Qenna (*.qtheme)");
}

QString suggestedFileName(const Theme::MiraTheme& theme)
{
    QString base = theme.name.trimmed();
    if (base.isEmpty()) base = QCoreApplication::translate("ThemePackage", "tema");
    // Tira o que o Windows não aceita em nome de arquivo, além de barras.
    base.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QString());
    base = base.simplified();
    if (base.isEmpty()) base = QCoreApplication::translate("ThemePackage", "tema");
    return base + QLatin1Char('.') + extension();
}

bool exportToFile(const Theme::MiraTheme& theme, const QString& path, QString* error)
{
    const auto fail = [&](const QString& msg) {
        if (error) *error = msg;
        return false;
    };

    Theme::MiraTheme out = theme;

    // Todo tema que sai daqui precisa de identidade própria e estável — é ela
    // que permite reconhecer o mesmo tema depois, mesmo renomeado.
    if (out.uuid.isEmpty())
        out.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);

    ZipWriter zip;

    // --- imagem de fundo ---
    QByteArray imageBytes;
    QString imageEntryName;
    if (!out.backgroundImage.isEmpty()) {
        QFile img(out.backgroundImage);
        const QString suffix = QFileInfo(out.backgroundImage).suffix().toLower();
        if (img.exists() && isAllowedImageSuffix(suffix) && img.open(QIODevice::ReadOnly)) {
            imageBytes = img.read(kMaxPackageBytes);
            img.close();
        }
        if (!imageBytes.isEmpty()) {
            imageEntryName = QStringLiteral("background.") + suffix;
            // Dentro do pacote o campo é só o nome do item no zip. Guardar o
            // caminho original aqui exporia algo como
            // "C:/Users/<nome da pessoa>/..." pra quem baixasse o tema.
            out.backgroundImage = imageEntryName;
        } else {
            // A imagem sumiu do disco ou é de um tipo que não empacotamos.
            // Melhor exportar um tema sem fundo do que um tema apontando pra
            // um caminho que não existe em lugar nenhum.
            out.backgroundImage.clear();
        }
    }

    // --- envelope ---
    QJsonObject root;
    root[QStringLiteral("kind")] = kindTag();
    root[QStringLiteral("format")] = kFormatVersion;
    root[QStringLiteral("app")] = QStringLiteral("Qenna Writer");
    root[QStringLiteral("appVersion")] = QStringLiteral(APP_VERSION);
    root[QStringLiteral("exportedAt")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    root[QStringLiteral("theme")] = Theme::themeToJson(out);

    zip.addFile(QString::fromLatin1(kThemeJsonName),
                QJsonDocument(root).toJson(QJsonDocument::Indented), /*compress=*/true);

    // Imagem já vem comprimida (PNG/JPEG/WebP); deflate em cima disso só
    // gastaria tempo pra não encolher quase nada.
    if (!imageBytes.isEmpty())
        zip.addFile(imageEntryName, imageBytes, /*compress=*/false);

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return fail(QCoreApplication::translate("ThemePackage", "Não foi possível gravar o arquivo em: %1").arg(QDir::toNativeSeparators(path)));
    const QByteArray blob = zip.finish();
    if (f.write(blob) != blob.size()) {
        f.close();
        return fail(QCoreApplication::translate("ThemePackage", "Não foi possível gravar o arquivo em: %1").arg(QDir::toNativeSeparators(path)));
    }
    f.close();
    return true;
}

bool importFromFile(const QString& path, ImportResult* out, QString* error)
{
    const auto fail = [&](const QString& msg) {
        if (error) *error = msg;
        return false;
    };
    if (!out) return fail(QCoreApplication::translate("ThemePackage", "Erro interno ao importar o tema."));

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return fail(QCoreApplication::translate("ThemePackage", "Não foi possível abrir o arquivo: %1").arg(QDir::toNativeSeparators(path)));
    if (f.size() > kMaxPackageBytes) {
        f.close();
        return fail(QCoreApplication::translate("ThemePackage", "O arquivo é grande demais pra ser um tema (acima de 64 MB)."));
    }
    const QByteArray blob = f.readAll();
    f.close();

    ZipReader zip;
    QString zipError;
    if (!zip.open(blob, &zipError))
        return fail(QCoreApplication::translate("ThemePackage", "Este arquivo não parece ser um tema do Qenna. (%1)").arg(zipError));

    bool ok = false;
    const QByteArray jsonBytes = zip.fileData(QString::fromLatin1(kThemeJsonName), &ok, &zipError);
    if (!ok)
        return fail(QCoreApplication::translate("ThemePackage", "Este arquivo não parece ser um tema do Qenna. (%1)").arg(zipError));

    QJsonParseError perr {};
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject())
        return fail(QCoreApplication::translate("ThemePackage", "O tema está corrompido e não pôde ser lido."));

    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("kind")).toString() != kindTag())
        return fail(QCoreApplication::translate("ThemePackage", "Este arquivo não é um tema do Qenna."));
    if (!root.value(QStringLiteral("theme")).isObject())
        return fail(QCoreApplication::translate("ThemePackage", "O tema está corrompido e não pôde ser lido."));

    ImportResult result;
    result.theme = Theme::themeFromJson(root.value(QStringLiteral("theme")).toObject());
    result.theme.bundled = false;
    // `id` é local à máquina: quem chama decide o dele. O uuid é o que
    // identifica o tema no mundo, e esse a gente preserva.
    result.theme.id.clear();
    if (result.theme.uuid.isEmpty())
        result.theme.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (result.theme.name.trimmed().isEmpty())
        result.theme.name = QCoreApplication::translate("ThemePackage", "Tema importado");

    // Aviso (não erro) quando o pacote é mais novo que este app: importamos
    // assim mesmo, com os campos desconhecidos preservados em extras.
    const int fmt = root.value(QStringLiteral("format")).toInt(1);
    const QString minApp = result.theme.minAppVersion;
    if (fmt > kFormatVersion) {
        result.warning = QCoreApplication::translate("ThemePackage", "Este tema foi criado numa versão mais nova do Qenna. "
                             "Ele foi importado, mas pode ter detalhes que esta versão ainda não mostra.");
    } else if (!minApp.isEmpty() && compareVersions(QStringLiteral(APP_VERSION), minApp) < 0) {
        result.warning = QCoreApplication::translate("ThemePackage", "Este tema pede o Qenna %1 ou mais novo. "
                             "Ele foi importado, mas pode não aparecer exatamente como o autor fez.").arg(minApp);
    }

    // --- imagem de fundo ---
    const QString entry = result.theme.backgroundImage;
    result.theme.backgroundImage.clear();
    if (!entry.isEmpty()) {
        // O campo vem de arquivo de terceiro: só aceitamos um nome simples de
        // item do próprio zip. Sem barra, sem "..", sem caminho absoluto —
        // senão um pacote malicioso poderia mandar gravar em qualquer lugar
        // do disco ("../../algo/importante").
        const bool nameIsSafe = !entry.contains(QLatin1Char('/'))
                             && !entry.contains(QLatin1Char('\\'))
                             && !entry.contains(QStringLiteral(".."))
                             && !QDir::isAbsolutePath(entry);
        const QString suffix = QFileInfo(entry).suffix().toLower();

        if (nameIsSafe && isAllowedImageSuffix(suffix) && zip.contains(entry)) {
            bool imgOk = false;
            const QByteArray imgBytes = zip.fileData(entry, &imgOk, &zipError);
            // Decodificar de verdade é a única prova de que é imagem — a
            // extensão é só uma promessa de quem empacotou.
            QImage probe;
            if (imgOk && !imgBytes.isEmpty() && probe.loadFromData(imgBytes)) {
                const QString dir = importedImagesDir();
                if (QDir().mkpath(dir)) {
                    const QString dest = QDir::cleanPath(
                        dir + QLatin1Char('/') + result.theme.uuid + QLatin1Char('.') + suffix);
                    QFile imgOut(dest);
                    if (imgOut.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                        if (imgOut.write(imgBytes) == imgBytes.size())
                            result.theme.backgroundImage = dest;
                        imgOut.close();
                    }
                }
            }
        }
        // Se a imagem não veio, não era imagem, ou não deu pra gravar: o tema
        // entra sem fundo em vez de falhar. As cores são o essencial.
        if (result.theme.backgroundImage.isEmpty() && result.warning.isEmpty())
            result.warning = QCoreApplication::translate("ThemePackage", "A imagem de fundo deste tema não pôde ser carregada. "
                                 "O resto do tema foi importado normalmente.");
    }

    *out = result;
    return true;
}

} // namespace ThemePackage
