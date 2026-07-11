#include "Quotes.h"

#include <QCoreApplication>
#include <QRandomGenerator>
#include <QSettings>
#include <QString>
#include <QStringList>

namespace {

#include "_quotes_data.inc"

void shuffleInPlace(QStringList& list)
{
    auto* rng = QRandomGenerator::global();
    for (int i = list.size() - 1; i > 0; --i) {
        const int j = int(rng->bounded(quint32(i + 1)));
        if (i != j) list.swapItemsAt(i, j);
    }
}

// Guarda o texto original (não traduzido) — o ciclo embaralhado é persistido em
// QSettings entre sessões, então traduzir aqui prenderia o cache no idioma de
// quando foi gerado. A tradução acontece na leitura, em next().
QStringList toList(const char* const* arr, int count)
{
    QStringList out;
    out.reserve(count);
    for (int i = 0; i < count; ++i) out << QString::fromUtf8(arr[i]);
    return out;
}

// Ciclo ponderado: [feat1, feat2, reg, feat1, feat2, reg, ...] com cada
// pool embaralhado. Espelha buildWeightedQuoteCycle do Mira 1.
QStringList buildWeightedCycle()
{
    QStringList reg   = toList(kQuotesRegular, kQuotesRegular_count);
    QStringList feat1 = toList(kQuotesFeature, kQuotesFeature_count);
    QStringList feat2 = toList(kQuotesFeature, kQuotesFeature_count);
    shuffleInPlace(reg);
    shuffleInPlace(feat1);
    shuffleInPlace(feat2);

    if (reg.isEmpty() && feat1.isEmpty()) return {};
    if (feat1.isEmpty()) return reg;
    if (reg.isEmpty())   return feat1 + feat2;

    QStringList cycle;
    cycle.reserve(reg.size() + feat1.size() + feat2.size());
    int ri = 0, f1 = 0, f2 = 0;
    while (ri < reg.size() || f1 < feat1.size() || f2 < feat2.size()) {
        if (f1 < feat1.size()) cycle << feat1.at(f1++);
        if (f2 < feat2.size()) cycle << feat2.at(f2++);
        if (ri < reg.size())   cycle << reg.at(ri++);
    }
    return cycle;
}

constexpr auto kCycleKey   = "quotes/cycle";
constexpr auto kPointerKey = "quotes/pointer";

} // namespace

namespace Quotes {

QString pickRandom()
{
    auto* rng = QRandomGenerator::global();
    const int total = kQuotesRegular_count + 2 * kQuotesFeature_count;
    if (total <= 0) return {};
    const int pick = int(rng->bounded(quint32(total)));
    if (pick < kQuotesRegular_count) {
        return QCoreApplication::translate("Quotes", kQuotesRegular[pick]);
    }
    const int featIdx = (pick - kQuotesRegular_count) % kQuotesFeature_count;
    return QCoreApplication::translate("Quotes", kQuotesFeature[featIdx]);
}

QString next()
{
    QSettings settings;
    QStringList cycle = settings.value(QLatin1String(kCycleKey)).toStringList();
    int ptr = settings.value(QLatin1String(kPointerKey), 0).toInt();

    if (cycle.isEmpty() || ptr < 0 || ptr >= cycle.size()) {
        cycle = buildWeightedCycle();
        ptr = 0;
        settings.setValue(QLatin1String(kCycleKey), cycle);
    }
    if (cycle.isEmpty()) return {};

    const QString quote = cycle.at(ptr);
    ++ptr;
    settings.setValue(QLatin1String(kPointerKey), ptr);
    return QCoreApplication::translate("Quotes", quote.toUtf8().constData());
}

int regularCount() { return kQuotesRegular_count; }
int featureCount() { return kQuotesFeature_count; }

} // namespace Quotes
