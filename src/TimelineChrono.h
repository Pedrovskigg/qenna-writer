#pragma once

// Parser cronológico de marcadores livres de tempo ("Dia 5", "15/05/2026",
// "Verão de 1999", "há 10 anos"...) usado pelo eixo História da Timeline.
// Extraído de TimelineScene.cpp (onde nasceu) pra ficar reutilizável também
// em TimelinePanel.cpp.
//
// Entende português e, desde o rework do Gerador de Timeline (2026-09-27),
// também as formas equivalentes em inglês, espanhol, italiano e francês
// ("Day 5", "3 years ago", "hace 3 años", "3 anni fa", "il y a 3 ans"), e o
// relativo "N anos antes" (antes só "há N anos" contava; "20 anos antes"
// virava meio dia antes do começo). O que já era entendido continua dando o
// mesmo valor.

#include <QDate>
#include <QHash>
#include <QRegularExpression>
#include <QString>

namespace TimelineChrono {

// Lê um marcador de relógio em minutos do dia: "02:00", "23h04", "14h30", "9h".
// Retorna -1 se o marcador não for um horário reconhecível (ex.: "Dia 5", "Inverno").
inline qreal clockMinutes(const QString& marker)
{
    if (marker.isEmpty()) return -1.0;
    static const QRegularExpression reHM(QStringLiteral("(\\d{1,2})\\s*[:h]\\s*(\\d{2})"));
    static const QRegularExpression reH(QStringLiteral("\\b(\\d{1,2})\\s*h(?![\\dh])"));
    QRegularExpressionMatch m = reHM.match(marker);
    if (m.hasMatch()) {
        const int hh = m.captured(1).toInt();
        const int mm = m.captured(2).toInt();
        if (hh <= 47 && mm <= 59) return hh * 60.0 + mm;
    }
    m = reH.match(marker);
    if (m.hasMatch()) {
        const int hh = m.captured(1).toInt();
        if (hh <= 47) return hh * 60.0;
    }
    return -1.0;
}

// Converte um numeral romano (i..mmm) em inteiro; 0 se inválido.
inline int romanToInt(const QString& s)
{
    static const QHash<QChar, int> val{
        {'i',1},{'v',5},{'x',10},{'l',50},{'c',100},{'d',500},{'m',1000}};
    int total = 0, prev = 0;
    for (int i = s.size() - 1; i >= 0; --i) {
        const int v = val.value(s.at(i).toLower(), -1);
        if (v < 0) return 0;
        total += (v < prev) ? -v : v;
        prev = v;
    }
    return total;
}

// Unidade de tempo escrita ("anos", "weeks", "días", "giorni", "mois"...) →
// dias e minutos. false se a palavra não é uma unidade conhecida.
inline bool unitSpan(const QString& u, qreal* days, qreal* minutes)
{
    *days = 0.0; *minutes = 0.0;
    auto any = [&](std::initializer_list<const char*> ws) {
        for (const char* w : ws) if (u == QString::fromUtf8(w)) return true;
        return false;
    };
    if (any({"ano","anos","year","years","año","años","anno","anni","an","ans","année","années"})) { *days = 365.0; return true; }
    if (any({"mês","mes","meses","month","months","mese","mesi","mois"}))                        { *days = 30.0;  return true; }
    if (any({"semana","semanas","week","weeks","settimana","settimane","semaine","semaines"}))    { *days = 7.0;   return true; }
    if (any({"dia","dias","day","days","día","días","giorno","giorni","jour","jours"}))           { *days = 1.0;   return true; }
    if (any({"hora","horas","hour","hours","ora","ore","heure","heures"}))                        { *minutes = 60.0; return true; }
    if (any({"minuto","minutos","minute","minutes","minuti"}))                                    { *minutes = 1.0; return true; }
    return false;
}

// Alternância de unidades usada nos regex (a ordem importa: plural antes).
inline QString unitPattern()
{
    return QStringLiteral("(anos|ano|years|year|años|año|anni|anno|années|année|ans|an|"
                          "meses|mês|mes|months|month|mesi|mese|mois|"
                          "semanas|semana|weeks|week|settimane|settimana|semaines|semaine|"
                          "dias|dia|days|day|días|día|giorni|giorno|jours|jour|"
                          "horas|hora|hours|hour|ore|ora|heures|heure|"
                          "minutos|minuto|minutes|minute|minuti)");
}

// O que o parser entendeu, em partes — alimenta a "leitura ao vivo" da Mesa
// de preenchimento, além do escalar de sempre.
struct Parsed {
    bool   ok = false;       // reconheceu ALGO
    bool   weak = false;     // só os empurrões fracos de "antes"/"depois"
    bool   relative = false; // "há N anos", "N anos antes", "3 years ago"...
    qreal  value = 0.0;      // minutos (1 dia = 1440)
    int    dayNumber = -1;   // "Dia N" → N
    QDate  date;             // data de calendário, quando houver
    QString period;          // palavra de período/hora achada ("noite", "21:00")
};

// Parser cronológico: transforma um marcador livre num escalar comparável (em
// minutos, com 1 dia = 1440). Entende relógio, períodos do dia, "Dia N",
// "Ano N"/romano, datas (DD/MM[/AAAA], AAAA-MM-DD), estações e "há N anos/dias".
inline Parsed parseDetailed(const QString& raw)
{
    constexpr qreal DAY = 1440.0;
    Parsed r;
    const QString s = raw.toLower().trimmed();
    bool ok = false, strong = false;
    qreal days = 0.0, minutes = 0.0;

    if (s.isEmpty()) return r;
    constexpr auto U = QRegularExpression::UseUnicodePropertiesOption;

    // ── hora explícita (02:00, 23h04, 9h) ─────────────────────────────────────
    const qreal clk = clockMinutes(s);
    if (clk >= 0.0) {
        minutes = clk; ok = strong = true;
        r.period = QStringLiteral("%1:%2").arg(int(clk) / 60, 2, 10, QLatin1Char('0'))
                                          .arg(int(clk) % 60, 2, 10, QLatin1Char('0'));
    } else {
        // ── período do dia (sem hora explícita) ───────────────────────────────
        // Compostos antes das palavras que eles contêm ("entardecer" antes de
        // "tarde", "midnight" antes de "night"). As do português casam por
        // "contém", como sempre; as outras línguas exigem palavra inteira.
        struct P { const char* w; qreal m; bool whole; };
        static const P periods[] = {
            {"meia-noite",0,false},{"meia noite",0,false},{"madrugada",180,false},{"alvorada",330,false},
            {"amanhecer",360,false},{"nascer do sol",360,false},{"entardecer",1050,false},
            {"manhã",540,false},{"manha",540,false},{"meio-dia",720,false},{"meio dia",720,false},
            {"pôr do sol",1080,false},{"por do sol",1080,false},{"anoitecer",1110,false},
            {"crepúsculo",1110,false},{"crepusculo",1110,false},{"tarde",900,false},{"noite",1260,false},
            // inglês
            {"midnight",0,true},{"dawn",330,true},{"sunrise",360,true},{"morning",540,true},
            {"afternoon",900,true},{"noon",720,true},{"midday",720,true},{"sunset",1080,true},
            {"dusk",1110,true},{"evening",1170,true},{"night",1260,true},
            // espanhol
            {"medianoche",0,true},{"atardecer",1050,true},{"anochecer",1110,true},
            {"mañana",540,true},{"mediodía",720,true},{"noche",1260,true},
            // italiano
            {"mezzanotte",0,true},{"alba",330,true},{"mattina",540,true},{"mattino",540,true},
            {"mezzogiorno",720,true},{"pomeriggio",900,true},{"tramonto",1080,true},
            {"sera",1170,true},{"notte",1260,true},
            // francês
            {"minuit",0,true},{"aube",330,true},{"après-midi",900,true},{"matin",540,true},
            {"midi",720,true},{"crépuscule",1110,true},{"soir",1170,true},{"nuit",1260,true}};
        for (const auto& p : periods) {
            const QString w = QString::fromUtf8(p.w);
            bool hit = false;
            if (!p.whole) hit = s.contains(w);
            else {
                static QHash<QString, QRegularExpression> cache;
                auto it = cache.find(w);
                if (it == cache.end())
                    it = cache.insert(w, QRegularExpression(QStringLiteral("(?<![\\w-])") + QRegularExpression::escape(w)
                                                            + QStringLiteral("(?![\\w-])"), U));
                hit = it->match(s).hasMatch();
            }
            if (hit) { minutes = p.m; ok = strong = true; r.period = w; break; }
        }
    }

    // ── relativos ao começo: "há N anos", "N years ago", "hace N años",
    //    "N anni fa", "il y a N ans" e "N anos antes / before / prima / avant" ──
    {
        static const QList<QRegularExpression> agoForms = {
            QRegularExpression(QStringLiteral("h[áa]\\s+(\\d+)\\s*") + unitPattern(), U),
            QRegularExpression(QStringLiteral("hace\\s+(\\d+)\\s*") + unitPattern(), U),
            QRegularExpression(QStringLiteral("il\\s+y\\s+a\\s+(\\d+)\\s*") + unitPattern(), U),
            QRegularExpression(QStringLiteral("(\\d+)\\s*") + unitPattern() + QStringLiteral("\\s+(?:ago|fa|antes|atr[áa]s|before|earlier|prima|avant)\\b"), U),
        };
        for (const auto& re : agoForms) {
            const auto m = re.match(s);
            if (!m.hasMatch()) continue;
            qreal ud = 0, um = 0;
            if (!unitSpan(m.captured(2), &ud, &um)) continue;
            const qreal n = m.captured(1).toDouble();
            days -= n * ud; minutes -= n * um;
            ok = strong = true; r.relative = true;
            break;
        }
    }

    // ── datas absolutas ────────────────────────────────────────────────────────
    bool dated = false;
    {
        static const QRegularExpression reYMD(QStringLiteral("(\\d{4})-(\\d{1,2})-(\\d{1,2})"));
        const auto m = reYMD.match(s);
        if (m.hasMatch()) {
            const int y = m.captured(1).toInt(), mo = m.captured(2).toInt(), d = m.captured(3).toInt();
            days += y * 372.0 + (mo - 1) * 31.0 + (d - 1); ok = strong = dated = true;
            r.date = QDate(y, mo, d);
        }
    }
    if (!dated) {
        static const QRegularExpression reDMY(QStringLiteral("\\b(\\d{1,2})[/.](\\d{1,2})(?:[/.](\\d{2,4}))?\\b"));
        const auto m = reDMY.match(s);
        if (m.hasMatch()) {
            const int d = m.captured(1).toInt(), mo = m.captured(2).toInt();
            int y = m.captured(3).isEmpty() ? 0 : m.captured(3).toInt();
            if (y > 0 && y < 100) y += 2000; // "23" → 2023
            days += y * 372.0 + (mo - 1) * 31.0 + (d - 1); ok = strong = dated = true;
            if (y > 0) r.date = QDate(y, mo, d);
        }
    }

    // ── "Dia N" / "Dia seguinte" ──────────────────────────────────────────────
    if (!dated) {
        static const QRegularExpression reDia(QStringLiteral("\\b(?:dia|day|día|giorno|jour)\\s+(\\d+)"), U);
        const auto m = reDia.match(s);
        if (m.hasMatch()) {
            days += m.captured(1).toDouble() - 1.0; ok = strong = true;
            r.dayNumber = m.captured(1).toInt();
        } else {
            static const QRegularExpression reNext(QStringLiteral(
                "seguinte|next day|following day|día siguiente|giorno dopo|giorno seguente|lendemain|jour suivant"), U);
            if (reNext.match(s).hasMatch()) { days += 1.0; ok = strong = true; }
        }
    }

    // ── "Ano N" / "Ano III" (e year/año/anno/année) ───────────────────────────
    {
        static const QRegularExpression reAnoN(QStringLiteral("\\b(?:ano|year|año|anno|année)\\s+(\\d+)"), U);
        const auto mN = reAnoN.match(s);
        if (mN.hasMatch()) { days += mN.captured(1).toDouble() * 372.0; ok = strong = true; }
        else {
            static const QRegularExpression reAnoR(QStringLiteral("\\b(?:ano|year|año|anno|année)\\s+([ivxlcdm]+)\\b"), U);
            const auto mR = reAnoR.match(s);
            if (mR.hasMatch()) {
                const int n = romanToInt(mR.captured(1));
                if (n > 0) { days += n * 372.0; ok = strong = true; }
            }
        }
    }

    // ── estações (aprox. por dia do ano) ──────────────────────────────────────
    {
        struct Season { QString w; qreal d; bool pt; QRegularExpression re; };
        static const QList<Season> seasons = [&]() {
            QList<Season> l;
            auto add = [&](const char* w, qreal d, bool pt) {
                const QString ws = QString::fromUtf8(w);
                l.append({ ws, d, pt, pt ? QRegularExpression()
                                         : QRegularExpression(QStringLiteral("(?<!\\w)") + ws + QStringLiteral("(?!\\w)"), U) });
            };
            // as do português casam por "contém" (como sempre); as outras, palavra inteira
            add("primavera", 80, true); add("spring", 80, false); add("printemps", 80, false);
            add("verão", 172, true); add("verao", 172, true); add("summer", 172, false);
            add("verano", 172, false); add("estate", 172, false); add("été", 172, false);
            add("outono", 266, true); add("autumn", 266, false); add("otoño", 266, false);
            add("autunno", 266, false); add("automne", 266, false);
            add("inverno", 355, true); add("winter", 355, false); add("invierno", 355, false); add("hiver", 355, false);
            return l;
        }();
        for (const auto& se : seasons) {
            const bool hit = se.pt ? s.contains(se.w) : se.re.match(s).hasMatch();
            if (hit) { days += se.d; ok = strong = true; break; }
        }
    }

    // ── empurrões relativos fracos (só quando não é "N anos antes") ───────────
    if (!r.relative) {
        if (s.contains(QStringLiteral("antes")))  { days -= 0.5; ok = true; }
        if (s.contains(QStringLiteral("depois"))) { days += 0.5; ok = true; }
    }

    r.ok = ok;
    r.weak = ok && !strong;
    r.value = days * DAY + minutes;
    return r;
}

// okOut = true se reconheceu ALGO. Marcadores não reconhecidos → ordem manual.
inline qreal parse(const QString& raw, bool* okOut)
{
    const Parsed p = parseDetailed(raw);
    if (okOut) *okOut = p.ok;
    return p.value;
}

} // namespace TimelineChrono
