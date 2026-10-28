#pragma once

// Cronômetro por trecho do Mapa: CARTO_PROF("nome") mede o escopo e soma num
// relógio global. Só a bancada de desempenho lê (carto::profReport); no app o
// custo é um relógio por trecho grande, nada por pixel.

#include <QElapsedTimer>
#include <QString>

#include <map>
#include <mutex>
#include <string>

namespace carto {

struct ProfEntry { double ms = 0; int calls = 0; };
inline std::map<std::string, ProfEntry>& profTable() { static std::map<std::string, ProfEntry> t; return t; }
inline std::mutex& profMutex() { static std::mutex m; return m; }
inline bool& profOn() { static bool on = false; return on; }

class ProfScope {
public:
    explicit ProfScope(const char* name) : m_name(name) { if (profOn()) m_t.start(); }
    ~ProfScope()
    {
        if (!profOn() || !m_t.isValid()) return;
        const double ms = m_t.nsecsElapsed() / 1e6;
        std::lock_guard<std::mutex> lock(profMutex());
        ProfEntry& e = profTable()[m_name];
        e.ms += ms;
        e.calls++;
    }
private:
    const char* m_name;
    QElapsedTimer m_t;
};

inline QString profReport(double minMs = 2)
{
    std::lock_guard<std::mutex> lock(profMutex());
    QString out;
    for (const auto& [name, e] : profTable())
        if (e.ms >= minMs) out += QStringLiteral("      · %1 %2 ms (%3×)\n").arg(QString::fromStdString(name), -26).arg(e.ms, 8, 'f', 1).arg(e.calls);
    profTable().clear();
    return out;
}

} // namespace carto

#define CARTO_PROF_CAT2(a, b) a##b
#define CARTO_PROF_CAT(a, b) CARTO_PROF_CAT2(a, b)
#define CARTO_PROF(name) ::carto::ProfScope CARTO_PROF_CAT(_cartoProf, __LINE__)(name)
