#include "TimelineFillAssist.h"

#include "TimelineChrono.h"
#include "TimelineTracksTypes.h"

#include <QCoreApplication>
#include <QDate>
#include <QHash>
#include <QLocale>
#include <QRegularExpression>
#include <QStringList>

#include <iterator>

namespace FillAssist {

namespace {

constexpr auto kU = QRegularExpression::UseUnicodePropertiesOption;

// Números por extenso nas cinco línguas do app (até 29 nas formas fundidas,
// dezenas até cem). "vinte e quatro", "twenty-four", "treinta y dos",
// "vingt et un" saem por composição.
const QHash<QString, int>& numberWords()
{
    static const QHash<QString, int> w = []() {
        QHash<QString, int> h;
        auto add = [&](int n, std::initializer_list<const char*> ws) {
            for (const char* s : ws) h.insert(QString::fromUtf8(s), n);
        };
        add(1,  {"um","uma","a","an","one","un","uno","una","une"});
        add(2,  {"dois","duas","two","dos","due","deux"});
        add(3,  {"três","tres","three","tre","trois"});
        add(4,  {"quatro","four","cuatro","quattro","quatre"});
        add(5,  {"cinco","five","cinque","cinq"});
        add(6,  {"seis","six","sei"});
        add(7,  {"sete","seven","siete","sette","sept"});
        add(8,  {"oito","eight","ocho","otto","huit"});
        add(9,  {"nove","nine","nueve","neuf"});
        add(10, {"dez","ten","diez","dieci","dix"});
        add(11, {"onze","eleven","once","undici"});
        add(12, {"doze","twelve","doce","dodici","douze"});
        add(13, {"treze","thirteen","trece","tredici","treize"});
        add(14, {"catorze","quatorze","fourteen","catorce","quattordici"});
        add(15, {"quinze","fifteen","quince","quindici"});
        add(16, {"dezesseis","dezasseis","sixteen","dieciséis","dieciseis","sedici","seize"});
        add(17, {"dezessete","dezassete","seventeen","diecisiete","diciassette"});
        add(18, {"dezoito","eighteen","dieciocho","diciotto"});
        add(19, {"dezenove","dezanove","nineteen","diecinueve","diciannove"});
        add(20, {"vinte","twenty","veinte","venti","vingt"});
        add(21, {"veintiuno","veintiún","ventuno"});
        add(22, {"veintidós","ventidue"});
        add(23, {"veintitrés","ventitré","ventitre"});
        add(24, {"veinticuatro","ventiquattro"});
        add(25, {"veinticinco","venticinque"});
        add(26, {"veintiséis","ventisei"});
        add(27, {"veintisiete","ventisette"});
        add(28, {"veintiocho","ventotto"});
        add(29, {"veintinueve","ventinove"});
        add(30, {"trinta","thirty","treinta","trenta","trente"});
        add(40, {"quarenta","forty","cuarenta","quaranta","quarante"});
        add(50, {"cinquenta","fifty","cincuenta","cinquanta","cinquante"});
        add(60, {"sessenta","sixty","sesenta","sessanta","soixante"});
        add(70, {"setenta","seventy","settanta"});
        add(80, {"oitenta","eighty","ochenta","ottanta"});
        add(90, {"noventa","ninety","novanta"});
        add(100,{"cem","cento","hundred","cien","cent"});
        return h;
    }();
    return w;
}

// Número a partir de 1–3 palavras ("cinco", "24", "vinte e quatro"); -1 se não é.
int numberFrom(const QStringList& toks)
{
    if (toks.isEmpty()) return -1;
    const auto& W = numberWords();
    auto val = [&](const QString& t) -> int {
        bool isNum = false;
        const int n = t.toInt(&isNum);
        if (isNum) return n;
        return W.value(t, -1);
    };
    if (toks.size() == 1) return val(toks[0]);
    const bool conj = toks.size() == 3
        && (toks[1] == QLatin1String("e") || toks[1] == QLatin1String("y")
            || toks[1] == QLatin1String("et") || toks[1] == QLatin1String("and"));
    if (toks.size() == 2 || conj) {
        const int a = val(toks.first()), b = val(toks.last());
        if (a >= 10 && a <= 90 && a % 10 == 0 && b >= 1 && b <= 9) return a + b;
    }
    return -1;
}

bool unitOf(const QString& w, Unit* u)
{
    qreal d = 0, m = 0;
    if (!TimelineChrono::unitSpan(w, &d, &m) || d <= 0) return false;
    *u = d >= 365 ? Unit::Year : d >= 30 ? Unit::Month : d >= 7 ? Unit::Week : Unit::Day;
    return true;
}

// Trecho original (com maiúsculas) pro mesmo intervalo achado no texto minúsculo.
QString slice(const QString& orig, const QString& low, int from, int len)
{
    return orig.size() == low.size() ? orig.mid(from, len) : low.mid(from, len);
}

QString capFirst(QString s)
{
    if (!s.isEmpty()) s[0] = s[0].toUpper();
    return s;
}

struct Phrase { const char* text; const char* period; };

} // namespace

QString firstSentence(const QString& text)
{
    const QString t = text.trimmed();
    static const QRegularExpression end(QStringLiteral("[.!?…](?=\\s|$)|\\n"));
    const auto m = end.match(t);
    QString s = m.hasMatch() ? t.left(m.capturedStart() + 1) : t;
    if (s.size() > 240) s = s.left(240);
    return s.trimmed();
}

QString opening(const QString& text, int maxChars)
{
    // frases inteiras até maxChars; se a primeira já passa, corta na palavra
    const QString t = text.simplified();
    if (t.size() <= maxChars) return t;
    static const QRegularExpression end(QStringLiteral("[.!?…](?=\\s)"));
    int cut = -1;
    auto it = end.globalMatch(t);
    while (it.hasNext()) {
        const auto m = it.next();
        if (m.capturedEnd() > maxChars) break;
        cut = int(m.capturedEnd());
    }
    if (cut > 0) return t.left(cut);
    const int sp = t.lastIndexOf(QLatin1Char(' '), maxChars);
    return t.left(sp > 0 ? sp : maxChars) + QStringLiteral("…");
}

Reading reading(const QString& marker, bool startOk, qreal startChrono)
{
    Reading r;
    if (marker.trimmed().isEmpty()) return r;
    const TimelineChrono::Parsed p = TimelineChrono::parseDetailed(marker);
    if (!p.ok) {
        r.kind = Reading::Bad;
        r.text = QCoreApplication::translate("TimelineFill", "? não reconheço · segue a ordem dos capítulos");
        r.compact = QCoreApplication::translate("TimelineFill", "? não reconheço");
        return r;
    }
    if (p.weak) {
        r.kind = Reading::Bad;
        r.text = QCoreApplication::translate("TimelineFill", "só entendi “antes” ou “depois” · meio dia");
        r.compact = QCoreApplication::translate("TimelineFill", "só entendi “antes/depois”");
        return r;
    }
    r.kind = Reading::Ok;
    QString t, c;
    const bool before = p.relative && p.value < 0;
    if (before) {
        const QString g = Tracks::gapWord(-p.value / 1440.0);
        t = QCoreApplication::translate("TimelineFill", "→ %1 antes").arg(g);
        c = QStringLiteral("→ −") + g;
    } else if (p.dayNumber >= 0) {
        t = c = QCoreApplication::translate("TimelineFill", "→ dia %1").arg(p.dayNumber);
    } else if (p.date.isValid()) {
        t = c = QStringLiteral("→ ") + QLocale().toString(p.date, QLocale::ShortFormat);
    } else {
        t = c = QCoreApplication::translate("TimelineFill", "→ entendido");
    }
    if (!p.period.isEmpty() && !before) { t += QStringLiteral(", ") + p.period; c += QStringLiteral(", ") + p.period; }
    if (startOk && p.value < startChrono) { t += QCoreApplication::translate("TimelineFill", " · Flashback"); c += QCoreApplication::translate("TimelineFill", " · Flashback"); }
    r.text = t;
    r.compact = c;
    return r;
}

QString shifted(const QString& marker, int amount, Unit unit)
{
    const QString m = marker.trimmed();
    if (m.isEmpty()) return {};
    // "Dia 4", "Day 4", "Día 4"… (o que vem depois do número cai: ", noite")
    static const QRegularExpression reDay(QStringLiteral("\\b((?:dia|day|día|giorno|jour)\\s+)(\\d+)"),
                                          QRegularExpression::CaseInsensitiveOption | kU);
    if (const auto d = reDay.match(m); d.hasMatch()) {
        const int per = unit == Unit::Year ? 365 : unit == Unit::Month ? 30 : unit == Unit::Week ? 7 : 1;
        const int n = d.captured(2).toInt() + amount * per;
        if (n < 1) return {};
        return capFirst(d.captured(1)) + QString::number(n);
    }
    auto shiftDate = [&](QDate dt) {
        switch (unit) {
        case Unit::Year:  return dt.addYears(amount);
        case Unit::Month: return dt.addMonths(amount);
        case Unit::Week:  return dt.addDays(7 * amount);
        default:          return dt.addDays(amount);
        }
    };
    static const QRegularExpression reDMY(QStringLiteral("\\b(\\d{1,2})([/.])(\\d{1,2})\\2(\\d{4})\\b"));
    if (const auto d = reDMY.match(m); d.hasMatch()) {
        const QDate dt(d.captured(4).toInt(), d.captured(3).toInt(), d.captured(1).toInt());
        if (!dt.isValid()) return {};
        const QDate n = shiftDate(dt);
        const QChar sep = d.captured(2).at(0);
        return QStringLiteral("%1%2%3%2%4")
            .arg(n.day(), d.captured(1).size(), 10, QLatin1Char('0'))
            .arg(sep)
            .arg(n.month(), d.captured(3).size(), 10, QLatin1Char('0'))
            .arg(n.year());
    }
    static const QRegularExpression reYMD(QStringLiteral("\\b(\\d{4})-(\\d{1,2})-(\\d{1,2})\\b"));
    if (const auto d = reYMD.match(m); d.hasMatch()) {
        const QDate dt(d.captured(1).toInt(), d.captured(2).toInt(), d.captured(3).toInt());
        if (!dt.isValid()) return {};
        return shiftDate(dt).toString(QStringLiteral("yyyy-MM-dd"));
    }
    return {};
}

Suggestion suggest(const QString& text, const QString& prevMarker, const QString& prevLabel)
{
    Suggestion out;
    const QString orig = firstSentence(text);
    if (orig.isEmpty()) return out;
    const QString low = orig.toLower();

    auto relWhy = [&](const QString& phrase) {
        out.phrase = phrase;
        return QCoreApplication::translate("TimelineFill", "“%1” e o %2 é %3").arg(phrase, prevLabel, prevMarker.trimmed());
    };
    auto firstWhy = [&](const QString& phrase) {
        out.phrase = phrase;
        return QCoreApplication::translate("TimelineFill", "“%1” na primeira frase").arg(phrase);
    };
    auto findPhrase = [&](const Phrase* list, int n, int* at, QString* period) -> int {
        for (int i = 0; i < n; ++i) {
            const QString p = QString::fromUtf8(list[i].text);
            const QRegularExpression re(QStringLiteral("(?<![\\w'])") + QRegularExpression::escape(p)
                                        + QStringLiteral("(?![\\w])"), kU);
            const auto m = re.match(low);
            if (m.hasMatch()) { *at = int(m.capturedStart()); *period = QString::fromUtf8(list[i].period); return int(p.size()); }
        }
        return -1;
    };

    // ── mesmo dia do capítulo anterior ("naquela mesma noite") ────────────────
    static const Phrase sameDay[] = {
        {"naquela mesma noite","noite"},{"na mesma noite","noite"},{"naquela noite","noite"},
        {"naquela mesma tarde","tarde"},{"naquela tarde","tarde"},{"naquela mesma manhã","manhã"},
        {"naquela manhã","manhã"},{"mais tarde naquele dia",""},{"naquele mesmo dia",""},{"no mesmo dia",""},
        {"that same night","night"},{"later that night","night"},{"that night","night"},
        {"that evening","evening"},{"that afternoon","afternoon"},{"that morning","morning"},
        {"later that day",""},{"that same day",""},
        {"esa misma noche","noche"},{"esa noche","noche"},{"aquella noche","noche"},
        {"esa tarde","tarde"},{"esa mañana","mañana"},{"ese mismo día",""},{"más tarde ese día",""},
        {"quella stessa notte","notte"},{"quella notte","notte"},{"quella sera","sera"},
        {"quel pomeriggio","pomeriggio"},{"quella mattina","mattina"},{"quello stesso giorno",""},{"lo stesso giorno",""},
        {"cette même nuit","nuit"},{"cette nuit-là","nuit"},{"ce soir-là","soir"},
        {"cet après-midi-là","après-midi"},{"ce matin-là","matin"},{"le même jour",""},{"ce jour-là",""}};
    // ── dia seguinte ("na manhã seguinte") ────────────────────────────────────
    static const Phrase nextDay[] = {
        {"na manhã seguinte","manhã"},{"na noite seguinte","noite"},{"na tarde seguinte","tarde"},
        {"no dia seguinte",""},{"no outro dia",""},
        {"the following morning","morning"},{"the next morning","morning"},{"next morning","morning"},
        {"the next night","night"},{"the next day",""},{"the following day",""},{"the day after",""},
        {"a la mañana siguiente","mañana"},{"la noche siguiente","noche"},{"al día siguiente",""},{"al otro día",""},
        {"la mattina dopo","mattina"},{"la mattina seguente","mattina"},{"la notte dopo","notte"},
        {"il giorno dopo",""},{"il giorno seguente",""},{"l'indomani",""},
        {"le lendemain matin","matin"},{"le lendemain soir","soir"},{"le lendemain",""},{"le jour suivant",""}};

    for (int pass = 0; pass < 2; ++pass) {
        const Phrase* list = pass == 0 ? sameDay : nextDay;
        const int n = pass == 0 ? int(std::size(sameDay)) : int(std::size(nextDay));
        int at = -1; QString period;
        const int len = findPhrase(list, n, &at, &period);
        if (len < 0) continue;
        QString base = shifted(prevMarker, pass, Unit::Day);
        if (base.isEmpty()) return out; // sem base pra somar, não chuta
        if (!period.isEmpty()) base += QStringLiteral(", ") + period;
        out.marker = base;
        out.why = relWhy(slice(orig, low, at, len));
        return out;
    }

    const QString unit = TimelineChrono::unitPattern();
    auto numberBefore = [&](int end, int* startOut) -> int {
        // até três palavras antes da unidade: "vinte e quatro", "twenty-four"
        static const QRegularExpression word(QStringLiteral("[\\w']+"), kU);
        QStringList toks;
        QList<int> starts;
        auto wi = word.globalMatch(low.left(end));
        while (wi.hasNext()) { const auto w = wi.next(); toks << w.captured(); starts << int(w.capturedStart()); }
        for (int k = qMin(3, int(toks.size())); k >= 1; --k) {
            const int n = numberFrom(toks.mid(toks.size() - k));
            if (n > 0) { *startOut = starts[toks.size() - k]; return n; }
        }
        return -1;
    };

    // ── antes do começo: "doze anos antes", "há doze anos", "twelve years ago" ─
    {
        static const QRegularExpression suffix(QStringLiteral("(?<![\\w])") + TimelineChrono::unitPattern()
            + QStringLiteral("\\s+(antes|atrás|atras|before|earlier|ago|prima|avant|fa)(?![\\w])"), kU);
        static const QRegularExpression prefix(QStringLiteral("(?<![\\w])(há|ha|hace|il y a)\\s+"), kU);
        const auto m = suffix.match(low);
        if (m.hasMatch()) {
            int start = 0;
            const int nb = numberBefore(int(m.capturedStart()), &start);
            if (nb > 0) {
                out.marker = QStringLiteral("%1 %2 %3").arg(nb).arg(slice(orig, low, int(m.capturedStart(1)), int(m.capturedLength(1))).toLower(),
                                                            m.captured(2));
                out.why = firstWhy(slice(orig, low, start, int(m.capturedEnd()) - start));
                return out;
            }
        }
        // prefixo: "há N anos", "hace N años", "il y a N ans"
        auto pm = prefix.globalMatch(low);
        while (pm.hasNext()) {
            const auto p = pm.next();
            const QString rest = low.mid(p.capturedEnd());
            const QRegularExpression afterNum(QStringLiteral("^((?:[\\w']+[\\s-]+){1,3}?)") + unit
                                              + QStringLiteral("(?![\\w])"), kU);
            const auto am = afterNum.match(rest);
            if (!am.hasMatch()) continue;
            QString numTxt = am.captured(1).trimmed();
            numTxt.replace(QLatin1Char('-'), QLatin1Char(' '));
            const int n = numberFrom(numTxt.split(QLatin1Char(' '), Qt::SkipEmptyParts));
            if (n <= 0) continue;
            out.marker = capFirst(QStringLiteral("%1 %2 %3").arg(p.captured(1)).arg(n).arg(am.captured(2)));
            out.why = firstWhy(slice(orig, low, int(p.capturedStart()), int(p.capturedEnd() + am.capturedEnd() - p.capturedStart())));
            return out;
        }
    }

    // ── depois do capítulo anterior: "cinco dias depois", "three weeks later" ─
    {
        static const QRegularExpression after(QStringLiteral("(?<![\\w])") + TimelineChrono::unitPattern()
            + QStringLiteral("\\s+(depois|mais tarde|após|apos|later|after|afterwards|después|despues|más tarde|dopo|più tardi|plus tard|après)(?![\\w])"), kU);
        auto it = after.globalMatch(low);
        while (it.hasNext()) {
            const auto m = it.next();
            Unit u;
            if (!unitOf(m.captured(1), &u)) continue;
            int start = 0;
            const int n = numberBefore(int(m.capturedStart()), &start);
            if (n <= 0) continue;
            const QString base = shifted(prevMarker, n, u);
            if (base.isEmpty()) return out;
            out.marker = base;
            out.why = relWhy(slice(orig, low, start, int(m.capturedEnd()) - start));
            return out;
        }
    }
    return out;
}

} // namespace FillAssist
