#include "GalaxyNames.h"

#include <cctype>
#include <cmath>
#include <functional>
#include <initializer_list>
#include <utility>
#include <vector>

namespace galaxy {

namespace {
// o acaso daqui é uma variante do mulberry32 do núcleo: tem que seguir o Lab à risca
class NRng {
public:
    explicit NRng(uint32_t seed) : a(seed) {}
    double operator()()
    {
        a = a + 0x6d2b79f5u;
        uint32_t t = a;
        t = (t ^ (t >> 15)) * (t | 1u);
        t ^= t + (t ^ (t >> 7)) * (t | 61u);
        return double(t ^ (t >> 14)) / 4294967296.0;
    }
private:
    uint32_t a;
};
using List = std::vector<std::pair<const char*, double>>;
const char* pickW(NRng& r, const List& list)
{
    double s = 0;
    for (const auto& it : list) s += it.second;
    double x = r() * s;
    for (const auto& it : list) { x -= it.second; if (x <= 0) return it.first; }
    return list.back().first;
}
uint32_t hashStr(const std::string& s)
{
    uint32_t h = 2166136261u;
    for (unsigned char c : s) { h ^= c; h *= 16777619u; }
    return h;
}
// Sílabas: começo, vogal e fim.
const List ON { { "", 2 }, { "b", 0.8 }, { "c", 0.7 }, { "d", 1 }, { "f", 0.4 }, { "g", 0.7 }, { "h", 0.3 }, { "k", 1.3 }, { "l", 1.4 }, { "m", 1.1 }, { "n", 1.1 }, { "p", 0.6 }, { "r", 1.2 }, { "s", 1.2 }, { "t", 1.2 }, { "v", 1.1 }, { "z", 0.7 }, { "th", 0.9 }, { "kh", 0.4 }, { "zh", 0.25 }, { "sh", 0.4 }, { "ny", 0.2 }, { "qu", 0.25 }, { "x", 0.3 }, { "br", 0.3 }, { "dr", 0.45 }, { "kr", 0.35 }, { "tr", 0.45 }, { "vr", 0.2 }, { "gr", 0.3 } };
const List NU { { "a", 4 }, { "e", 3 }, { "i", 2.4 }, { "o", 2.6 }, { "u", 1 }, { "ae", 0.3 }, { "ai", 0.35 }, { "au", 0.25 }, { "ia", 0.3 }, { "io", 0.25 }, { "y", 0.35 }, { "ei", 0.2 } };
const List CO { { "", 7 }, { "n", 1.3 }, { "r", 1.3 }, { "s", 0.8 }, { "l", 1 }, { "m", 0.4 }, { "th", 0.25 }, { "x", 0.15 }, { "k", 0.3 } };
const List ON2 { { "l", 1 }, { "r", 1 }, { "n", 1 }, { "v", 1 }, { "th", 0.6 }, { "s", 1 } };
const List END_NONE { { "", 1 } };
const List END_SYS { { "", 6 }, { "is", 1 }, { "ar", 1 }, { "on", 1 }, { "ia", 1 }, { "ex", 0.4 }, { "ur", 0.6 }, { "eth", 0.6 }, { "ara", 0.8 }, { "ion", 0.6 }, { "os", 0.6 }, { "us", 0.5 }, { "ane", 0.4 }, { "ax", 0.3 } };
const List END_PLA { { "", 6 }, { "a", 1.2 }, { "is", 1 }, { "on", 1 }, { "ia", 0.9 }, { "or", 0.8 }, { "ea", 0.4 }, { "um", 0.5 }, { "es", 0.5 }, { "yn", 0.3 }, { "ith", 0.4 }, { "ora", 0.5 }, { "ax", 0.3 }, { "ul", 0.3 } };
const List END_MOON { { "", 6 }, { "a", 1.2 }, { "e", 0.8 }, { "is", 0.8 }, { "o", 0.6 }, { "ys", 0.3 }, { "ine", 0.4 }, { "el", 0.3 } };
const char* const ROMAN[] = { "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X", "XI", "XII" };
// Palavrões e palavras que não queremos ver num mapa (pt e en). Os curtos só contam no começo ou no fim.
const char* const BAD[] = { "merd", "bost", "foda", "fode", "porr", "caralh", "bucet", "xot", "piroc", "puta", "viad", "peid", "mija", "caga", "bund", "teta", "pint", "rola", "cuzao", "fuck", "shit", "cunt", "dick", "cock", "anal", "anus", "nazi", "sex", "nig", "fag", "rape", "piss", "tits", "boob", "porn", "damn", "kill", "dead", "morte", "slut", "whor", "bitch" };
const char* const BAD_START[] = { "cu", "pau", "pic", "put", "cul", "cag", "mij", "bos", "ass" };
const char* const BAD_END[] = { "cu", "ku", "pau", "cus", "kus", "ass" };
inline bool isVowel(char c) { return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'y'; }
inline bool endsVowel(const std::string& s) { return !s.empty() && isVowel(s.back()); }
inline bool startsWith(const std::string& s, const char* p) { return s.rfind(p, 0) == 0; }
inline bool endsWith(const std::string& s, const std::string& p) { return s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0; }
std::string cap(std::string s) { if (!s.empty()) s[0] = char(std::toupper(static_cast<unsigned char>(s[0]))); return s; }
std::string lower(std::string s) { for (char& c : s) c = char(std::tolower(static_cast<unsigned char>(c))); return s; }
std::string word(NRng& r, int syl, const List& ends)
{
    std::string s;
    for (int i = 0; i < syl; i++) {
        std::string on = pickW(r, ON);
        if (i > 0 && on.empty() && endsVowel(s)) on = pickW(r, ON2);
        s += on;
        s += pickW(r, NU);
        if (i != syl - 1) s += pickW(r, CO);
    }
    const std::string e = pickW(r, ends);
    if (!e.empty()) {
        if (endsVowel(s) && isVowel(e[0])) { s.pop_back(); s += e; }
        else s += e;
    }
    return s;
}
std::string take(uint32_t seed, std::unordered_set<std::string>& used, const std::string& k, const std::function<std::string(NRng&)>& base, const std::function<std::string(NRng&, const std::string&)>& suffix)
{
    NRng r((seed * 2654435761u) ^ hashStr(k));
    for (int t = 0; t < 80; t++) {
        const std::string w = base(r);
        if (!nameIsClean(w)) continue;
        const std::string n = suffix ? suffix(r, w) : w;
        if (!used.count(lower(n))) { used.insert(lower(n)); return n; }
    }
    const std::string n = cap(word(r, 2, END_PLA)) + "-" + std::to_string(100 + used.size());
    used.insert(lower(n));
    return n;
}
} // namespace

bool nameIsClean(const std::string& n)
{
    const std::string ln = lower(n);
    std::string l;
    for (char c : ln) if (c >= 'a' && c <= 'z') l += c;
    for (const char* b : BAD) if (l.find(b) != std::string::npos) return false;
    // cada palavra, separada pelo que não é letra
    std::string w;
    const auto check = [&]() {
        if (w.empty()) return true;
        for (const char* b : BAD_START) if (startsWith(w, b)) return false;
        for (const char* b : BAD_END) if (endsWith(w, b)) return false;
        return true;
    };
    for (char c : ln) {
        if (c >= 'a' && c <= 'z') w += c;
        else { if (!check()) return false; w.clear(); }
    }
    if (!check()) return false;
    for (size_t i = 0; i + 1 < l.size(); i++) if (isVowel(l[i]) && l[i] == l[i + 1]) return false;                 // vogal dobrada
    for (size_t i = 0; i + 2 < l.size(); i++) if (isVowel(l[i]) && isVowel(l[i + 1]) && isVowel(l[i + 2])) return false;   // três vogais seguidas
    // três consoantes seguidas, contando th, sh, kh, zh, qu e ny como uma só
    std::string q;
    for (size_t i = 0; i < l.size(); i++) {
        if (i + 1 < l.size()) {
            const char a = l[i], b = l[i + 1];
            if ((a == 't' && b == 'h') || (a == 's' && b == 'h') || (a == 'k' && b == 'h') || (a == 'z' && b == 'h') || (a == 'q' && b == 'u') || (a == 'n' && b == 'y')) { q += 'T'; i++; continue; }
        }
        q += l[i];
    }
    for (size_t i = 0; i + 2 < q.size(); i++) if (!isVowel(q[i]) && !isVowel(q[i + 1]) && !isVowel(q[i + 2])) return false;
    if (l.size() >= 2 && l[0] == l[1]) return false;
    return l.size() >= 4 && l.size() <= 10;
}

// Sistema (é também o nome da estrela). Uns 18% ganham número de catálogo; poucos têm duas palavras.
std::string NameGen::system(const std::string& k)
{
    return take(m_seed, m_used, k,
        [](NRng& r) -> std::string {
            if (r() < 0.1) { std::string a = cap(word(r, 1, END_NONE)); std::string b = cap(word(r, 2, END_SYS)); return a + " " + b; }
            const int syl = r() < 0.72 ? 2 : 3;
            return cap(word(r, syl, END_SYS));
        },
        [](NRng& r, const std::string& w) -> std::string {
            if (r() < 0.18 && w.find(' ') == std::string::npos) {
                const int num = r() < 0.5 ? int(std::floor(2 + r() * 98)) : int(std::floor(100 + r() * 900));
                return w + "-" + std::to_string(num);
            }
            return w;
        });
}
// Planeta: uns 20% com algarismo romano no fim, como Wilx IX.
std::string NameGen::planet(const std::string& k)
{
    return take(m_seed, m_used, k,
        [](NRng& r) -> std::string { const int syl = r() < 0.72 ? 2 : 3; return cap(word(r, syl, END_PLA)); },
        [](NRng& r, const std::string& w) -> std::string { if (r() < 0.2) return w + " " + ROMAN[int(std::floor(r() * 11))]; return w; });
}
// Lua: nomes curtos; de vez em quando uma letra (Argos B).
std::string NameGen::moon(const std::string& k)
{
    return take(m_seed, m_used, k,
        [](NRng& r) -> std::string { const int syl = r() < 0.7 ? 2 : 1; return cap(word(r, syl, END_MOON)); },
        [](NRng& r, const std::string& w) -> std::string { if (r() < 0.06) return w + " " + std::string(1, "BCD"[int(std::floor(r() * 3))]); return w; });
}

} // namespace galaxy
