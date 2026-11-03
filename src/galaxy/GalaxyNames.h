#pragma once

// Gerador de nomes da galáxia: nome próprio pra tudo que a geração cria (sistemas, planetas, luas).
// Som livre, de ficção científica; às vezes com número no fim, como se faz no espaço (Wilx IX, Kepler-22).
// Determinístico pela semente da galáxia; nunca repete; filtra palavrão em português e em inglês.

#include <cstdint>
#include <string>
#include <unordered_set>

namespace galaxy {

class NameGen {
public:
    explicit NameGen(uint32_t seed) : m_seed(seed) {}
    std::string system(const std::string& key);
    std::string planet(const std::string& key);
    std::string moon(const std::string& key);
private:
    uint32_t m_seed;
    std::unordered_set<std::string> m_used;
};
bool nameIsClean(const std::string& n);

} // namespace galaxy
