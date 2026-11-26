#include "PerfProbe.h"

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QMetaEnum>
#include <QSysInfo>
#include <QTextStream>
#include <QTimer>
#include <QWidget>

#include <algorithm>

namespace {

struct Stat {
    QString label;
    double totalMs = 0;
    double maxMs = 0;
    qint64 count = 0;
    bool top = false;      // entregue direto pelo laço de eventos (não aninhado)
};

struct Slow {
    double atS = 0;
    double ms = 0;
    QString label;
};

QElapsedTimer g_clock;
QHash<QPair<quintptr, int>, Stat> g_stats;
QList<Slow> g_slow;
int g_depth = 0;
double g_busyMs = 0;       // soma dos eventos de nível 0 = tempo em que o app esteve ocupado
double g_lastWriteS = 0;
double g_busyAtLastWrite = 0;

QString labelFor(QObject* r, QEvent::Type t)
{
    static const QMetaEnum types = QMetaEnum::fromType<QEvent::Type>();
    const char* tn = types.valueToKey(t);
    QString s = tn ? QString::fromLatin1(tn) : QStringLiteral("Event%1").arg(int(t));
    s += QStringLiteral(" → ") + QString::fromLatin1(r->metaObject()->className());
    if (!r->objectName().isEmpty()) s += QLatin1Char('#') + r->objectName();
    // timers e chamadas adiadas não dizem nada sozinhos: o dono ajuda
    QObject* p = r->parent();
    for (int i = 0; p && i < 2; ++i, p = p->parent()) {
        s += QStringLiteral(" ‹ ") + QString::fromLatin1(p->metaObject()->className());
        if (!p->objectName().isEmpty()) s += QLatin1Char('#') + p->objectName();
    }
    return s;
}

void writeReport()
{
    const double nowS = g_clock.elapsed() / 1000.0;
    QFile f(QDir::homePath() + QStringLiteral("/qenna-perf.txt"));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return;
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);

    const double span = nowS - g_lastWriteS;
    const double busy = g_busyMs - g_busyAtLastWrite;
    out << "Qenna perf  " << QDateTime::currentDateTime().toString(Qt::ISODate)
        << "  " << QSysInfo::prettyProductName() << "\n";
    out << QStringLiteral("Aberto há %1 s · ocupado no total %2 s · nos últimos %3 s: %4% ocupado\n\n")
               .arg(nowS, 0, 'f', 0).arg(g_busyMs / 1000.0, 0, 'f', 1)
               .arg(span, 0, 'f', 1).arg(span > 0 ? busy / (span * 10.0) : 0, 0, 'f', 0);
    g_lastWriteS = nowS;
    g_busyAtLastWrite = g_busyMs;

    QList<const Stat*> all;
    all.reserve(g_stats.size());
    for (const Stat& s : std::as_const(g_stats)) all << &s;
    std::sort(all.begin(), all.end(), [](const Stat* a, const Stat* b) { return a->totalMs > b->totalMs; });

    out << "== Tempo somado (ms total · vezes · média · pior) ==\n";
    for (int i = 0; i < all.size() && i < 60; ++i) {
        const Stat* s = all[i];
        out << QStringLiteral("%1  %2x  %3  %4  %5%6\n")
                   .arg(s->totalMs, 9, 'f', 1).arg(s->count, 7)
                   .arg(s->totalMs / qMax<qint64>(1, s->count), 7, 'f', 2)
                   .arg(s->maxMs, 7, 'f', 1)
                   .arg(s->top ? QString() : QStringLiteral("  "))
                   .arg(s->label);
    }
    out << "\n== Eventos isolados acima de 25 ms (mais recentes por último) ==\n";
    for (const Slow& s : std::as_const(g_slow))
        out << QStringLiteral("%1 s  %2 ms  %3\n").arg(s.atS, 8, 'f', 1).arg(s.ms, 7, 'f', 1).arg(s.label);
}

} // namespace

QennaApplication::QennaApplication(int& argc, char** argv)
    : QApplication(argc, argv)
    , m_perf(qEnvironmentVariableIntValue("QENNA_PERF") == 1)
{
    if (!m_perf) return;
    g_clock.start();
    auto* t = new QTimer(this);
    t->setInterval(3000);
    QObject::connect(t, &QTimer::timeout, this, [] { writeReport(); });
    t->start();
}

bool QennaApplication::notify(QObject* receiver, QEvent* event)
{
    if (!m_perf || !receiver || !event) return QApplication::notify(receiver, event);

    const QEvent::Type type = event->type();
    // O nome sai antes da entrega: o evento pode apagar o próprio destinatário.
    const auto key = qMakePair(quintptr(receiver), int(type));
    if (!g_stats.contains(key) && g_stats.size() <= 20000) {
        Stat s;
        s.label = labelFor(receiver, type);
        g_stats.insert(key, s);
    }

    QElapsedTimer t;
    t.start();
    ++g_depth;
    const bool r = QApplication::notify(receiver, event);
    --g_depth;
    const double ms = t.nsecsElapsed() / 1e6;

    if (g_depth == 0) g_busyMs += ms;
    if (ms < 0.02) return r;

    auto it = g_stats.find(key);
    if (it == g_stats.end()) return r;
    it->totalMs += ms;
    it->maxMs = std::max(it->maxMs, ms);
    ++it->count;
    if (g_depth == 0) it->top = true;

    if (ms >= 25.0) {
        g_slow.append({ g_clock.elapsed() / 1000.0, ms, it->label + (g_depth ? QStringLiteral("  (aninhado)") : QString()) });
        if (g_slow.size() > 80) g_slow.removeFirst();
    }
    return r;
}
