#include "ProjectTags.h"

#include <QHash>
#include <QRegularExpression>

#include <algorithm>

namespace ProjectTags {

namespace {
std::function<QStringList()>& provider()
{
    static std::function<QStringList()> fn;
    return fn;
}
}

QString normalize(const QString& raw)
{
    QString t = raw;
    t.remove(QLatin1Char('#'));
    static const QRegularExpression spaces(QStringLiteral("\\s+"));
    t.replace(spaces, QStringLiteral(" "));
    return t.trimmed().left(40).trimmed();
}

QStringList clean(const QStringList& tags)
{
    QStringList out;
    for (const QString& raw : tags) {
        const QString t = normalize(raw);
        if (!t.isEmpty() && !has(out, t)) out.append(t);
    }
    return out;
}

bool has(const QStringList& tags, const QString& tag)
{
    return tags.contains(tag, Qt::CaseInsensitive);
}

void setProvider(std::function<QStringList()> fn) { provider() = std::move(fn); }

QStringList vocabulary() { return provider() ? provider()() : QStringList(); }

QStringList rank(const QList<QStringList>& lists)
{
    QHash<QString, int> count;      // chave: minúscula
    QHash<QString, QString> spell;  // a primeira grafia
    for (const QStringList& l : lists)
        for (const QString& raw : l) {
            const QString t = normalize(raw);
            if (t.isEmpty()) continue;
            const QString k = t.toLower();
            if (!spell.contains(k)) spell.insert(k, t);
            count[k]++;
        }
    QStringList keys = spell.keys();
    std::sort(keys.begin(), keys.end(), [&](const QString& a, const QString& b) {
        if (count.value(a) != count.value(b)) return count.value(a) > count.value(b);
        return QString::localeAwareCompare(a, b) < 0;
    });
    QStringList out;
    for (const QString& k : keys) out.append(spell.value(k));
    return out;
}

QString canonical(const QString& tag, const QStringList& vocabulary)
{
    for (const QString& v : vocabulary)
        if (v.compare(tag, Qt::CaseInsensitive) == 0) return v;
    return tag;
}

} // namespace ProjectTags
