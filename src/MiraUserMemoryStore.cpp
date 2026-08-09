#include "MiraUserMemoryStore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QStandardPaths>
#include <QStringConverter>
#include <QTextStream>

namespace MiraUserMemoryStore {

namespace {

QString categoryLabel(const QString& category)
{
    static const QHash<QString, QString> kLabels = {
        { QStringLiteral("preferencia_criativa"), QObject::tr("Preferência criativa") },
        { QStringLiteral("processo"), QObject::tr("Jeito de trabalhar") },
        { QStringLiteral("ideia_solta"), QObject::tr("Ideia solta") },
        { QStringLiteral("vinculo"), QObject::tr("Vínculo") },
        { QStringLiteral("pendencia_geral"), QObject::tr("Pendência geral") },
    };
    return kLabels.value(category, category);
}

QString statusEmoji(const QString& status)
{
    static const QHash<QString, QString> kEmoji = {
        { QStringLiteral("confirmada"), QStringLiteral("🟢") },
        { QStringLiteral("em_discussao"), QStringLiteral("🟡") },
        { QStringLiteral("ideia_futura"), QStringLiteral("🔵") },
        { QStringLiteral("descartada"), QStringLiteral("🔴") },
    };
    return kEmoji.value(status, QStringLiteral("⚪"));
}

} // namespace

QString filePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/mira_user_memory.md");
}

QString load()
{
    QFile f(filePath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);
    return in.readAll();
}

int entryCount()
{
    const QString content = load();
    if (content.isEmpty()) return 0;
    int count = 0;
    for (const QString& line : content.split(QChar('\n'))) {
        if (line.startsWith(QStringLiteral("- **["))) ++count;
    }
    return count;
}

void appendEntry(const QString& category, const QString& status,
                  const QString& title, const QString& content)
{
    QFile f(filePath());
    if (!f.open(QIODevice::Append | QIODevice::Text)) return;
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out << QStringLiteral("- **[%1]** %2 **%3** — %4 _(%5)_\n\n")
        .arg(categoryLabel(category), statusEmoji(status), title, content,
             QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm")));
}

} // namespace MiraUserMemoryStore
