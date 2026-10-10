#include "NameAutocorrect.h"

#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <algorithm>

namespace {
const QString kSettingKey = QStringLiteral("editor/autocorrectNames");
int g_enabled = -1; // -1 = ainda não leu do QSettings

bool isApostrophe(QChar c) { return c == QLatin1Char('\'') || c == QChar(0x2019); }
}

bool NameAutocorrect::enabledSetting()
{
    if (g_enabled < 0) g_enabled = QSettings().value(kSettingKey, true).toBool() ? 1 : 0;
    return g_enabled == 1;
}

void NameAutocorrect::setEnabledSetting(bool on)
{
    g_enabled = on ? 1 : 0;
    QSettings().setValue(kSettingKey, on);
}

QString NameAutocorrect::fold(const QString& s)
{
    const QString d = s.normalized(QString::NormalizationForm_D);
    QString out;
    out.reserve(d.size());
    for (const QChar c : d) {
        const auto cat = c.category();
        if (cat == QChar::Mark_NonSpacing || cat == QChar::Mark_SpacingCombining || cat == QChar::Mark_Enclosing) continue;
        out.append(c.toLower());
    }
    return out;
}

void NameAutocorrect::setNames(const QStringList& names, const std::function<bool(const QString&)>& isWord)
{
    m_names.clear();
    QSet<QString> seen;
    static const QRegularExpression splitter(QStringLiteral("[^\\p{L}\\p{M}'\\x{2019}]+"));
    for (const QString& full : names) {
        for (QString w : full.split(splitter, Qt::SkipEmptyParts)) {
            w = withoutPossessive(w);
            while (!w.isEmpty() && isApostrophe(w.front())) w.remove(0, 1);
            if (w.size() < 3) continue;          // "Jo", "Li": perto demais de tudo
            const QString f = fold(w);
            if (seen.contains(f)) continue;      // sobrenome de família não vira empate
            seen.insert(f);
            Name n;
            n.text = w;
            n.folded = f;
            n.commonWord = isWord && isWord(w.toLower());
            m_names.append(n);
        }
    }
}

QString NameAutocorrect::withoutPossessive(const QString& word)
{
    if (word.size() > 2 && word.at(word.size() - 1).toLower() == QLatin1Char('s') && isApostrophe(word.at(word.size() - 2)))
        return word.left(word.size() - 2);
    if (word.size() > 1 && isApostrophe(word.back())) return word.left(word.size() - 1);
    return word;
}

int NameAutocorrect::distance(const QString& a, const QString& b, int cap)
{
    const int n = a.size(), m = b.size();
    if (std::abs(n - m) > cap) return cap + 1;
    // Três linhas bastam pra troca de vizinhas (alinhamento ótimo).
    QVector<int> prev2(m + 1), prev(m + 1), cur(m + 1);
    for (int j = 0; j <= m; ++j) prev[j] = j;
    for (int i = 1; i <= n; ++i) {
        cur[0] = i;
        int rowMin = cur[0];
        for (int j = 1; j <= m; ++j) {
            const int cost = a.at(i - 1) == b.at(j - 1) ? 0 : 1;
            int v = std::min({ prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + cost });
            if (i > 1 && j > 1 && a.at(i - 1) == b.at(j - 2) && a.at(i - 2) == b.at(j - 1))
                v = std::min(v, prev2[j - 2] + 1);
            cur[j] = v;
            rowMin = std::min(rowMin, v);
        }
        if (rowMin > cap) return cap + 1;
        std::swap(prev2, prev);
        std::swap(prev, cur);
    }
    return std::min(prev[m], cap + 1);
}

QString NameAutocorrect::correctionFor(const QString& word) const
{
    if (m_names.isEmpty()) return QString();
    const QString base = withoutPossessive(word);
    if (base.size() < 3) return QString();
    const QString suffix = word.mid(base.size());
    for (const QChar c : base)
        if (!c.isLetter() && !c.isMark()) return QString();   // "D'Ávila" e afins ficam de fora

    const QString typed = fold(base);
    const bool typedCapital = base.at(0).isUpper();
    int best = 99;
    const Name* bestName = nullptr;
    bool tie = false;
    for (const Name& n : m_names) {
        if (n.commonWord && !typedCapital) continue;
        const int cap = maxDistance(n.folded.size());
        const int d = distance(typed, n.folded, cap);
        if (d > cap) continue;
        if (d < best) { best = d; bestName = &n; tie = false; }
        else if (d == best) tie = true;
    }
    if (!bestName || tie || bestName->text == base) return QString();

    const bool allCaps = base.size() > 1 && base == base.toUpper();
    return (allCaps ? bestName->text.toUpper() : bestName->text) + suffix;
}
