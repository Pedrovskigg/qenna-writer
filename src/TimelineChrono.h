#pragma once

// Parser cronológico de marcadores livres de tempo ("Dia 5", "15/05/2026",
// "Verão de 1999", "há 10 anos"...) usado pelo eixo História da Timeline.
// Extraído de TimelineScene.cpp (onde nasceu) pra ficar reutilizável também
// em TimelinePanel.cpp — mesma lógica, sem alterações de comportamento.

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

// Parser cronológico: transforma um marcador livre num escalar comparável (em
// minutos, com 1 dia = 1440). Entende relógio, períodos do dia, "Dia N",
// "Ano N"/romano, datas (DD/MM[/AAAA], AAAA-MM-DD), estações e "há N anos/dias".
// okOut = true se reconheceu ALGO. Marcadores não reconhecidos → ordem manual.
inline qreal parse(const QString& raw, bool* okOut)
{
    constexpr qreal DAY = 1440.0;
    const QString s = raw.toLower().trimmed();
    bool ok = false;
    qreal days = 0.0, minutes = 0.0;

    if (s.isEmpty()) { if (okOut) *okOut = false; return 0.0; }

    // ── hora explícita (02:00, 23h04, 9h) ─────────────────────────────────────
    const qreal clk = clockMinutes(s);
    if (clk >= 0.0) { minutes = clk; ok = true; }
    else {
        // ── período do dia (sem hora explícita) ───────────────────────────────
        struct P { const char* w; qreal m; };
        static const P periods[] = {
            {"meia-noite",0},{"meia noite",0},{"madrugada",180},{"alvorada",330},
            {"amanhecer",360},{"nascer do sol",360},{"manhã",540},{"manha",540},
            {"meio-dia",720},{"meio dia",720},{"tarde",900},{"entardecer",1050},
            {"pôr do sol",1080},{"por do sol",1080},{"anoitecer",1110},
            {"crepúsculo",1110},{"crepusculo",1110},{"noite",1260}};
        for (const auto& p : periods)
            if (s.contains(QString::fromUtf8(p.w))) { minutes = p.m; ok = true; break; }
    }

    // ── "há N anos/meses/semanas/dias/horas/minutos" → passado (negativo) ─────
    {
        static const QRegularExpression re(QStringLiteral(
            "h[áa]\\s+(\\d+)\\s*(anos?|m[êe]s(?:es)?|semanas?|dias?|horas?|minutos?)"));
        const auto m = re.match(s);
        if (m.hasMatch()) {
            const qreal n = m.captured(1).toDouble();
            const QString u = m.captured(2);
            if      (u.startsWith(QStringLiteral("ano"))) days   -= n * 365.0;
            else if (u.startsWith(QStringLiteral("mê")) ||
                     u.startsWith(QStringLiteral("mes"))) days   -= n * 30.0;
            else if (u.startsWith(QStringLiteral("sem"))) days   -= n * 7.0;
            else if (u.startsWith(QStringLiteral("dia"))) days   -= n;
            else if (u.startsWith(QStringLiteral("hora"))) minutes -= n * 60.0;
            else if (u.startsWith(QStringLiteral("min")))  minutes -= n;
            ok = true;
        }
    }

    // ── datas absolutas ────────────────────────────────────────────────────────
    bool dated = false;
    {
        static const QRegularExpression reYMD(QStringLiteral("(\\d{4})-(\\d{1,2})-(\\d{1,2})"));
        const auto m = reYMD.match(s);
        if (m.hasMatch()) {
            const int y = m.captured(1).toInt(), mo = m.captured(2).toInt(), d = m.captured(3).toInt();
            days += y * 372.0 + (mo - 1) * 31.0 + (d - 1); ok = dated = true;
        }
    }
    if (!dated) {
        static const QRegularExpression reDMY(QStringLiteral("\\b(\\d{1,2})[/.](\\d{1,2})(?:[/.](\\d{2,4}))?\\b"));
        const auto m = reDMY.match(s);
        if (m.hasMatch()) {
            const int d = m.captured(1).toInt(), mo = m.captured(2).toInt();
            int y = m.captured(3).isEmpty() ? 0 : m.captured(3).toInt();
            if (y > 0 && y < 100) y += 2000; // "23" → 2023
            days += y * 372.0 + (mo - 1) * 31.0 + (d - 1); ok = dated = true;
        }
    }

    // ── "Dia N" / "Dia seguinte" ──────────────────────────────────────────────
    if (!dated) {
        static const QRegularExpression reDia(QStringLiteral("\\bdia\\s+(\\d+)"));
        const auto m = reDia.match(s);
        if (m.hasMatch()) { days += m.captured(1).toDouble() - 1.0; ok = true; }
        else if (s.contains(QStringLiteral("seguinte"))) { days += 1.0; ok = true; }
    }

    // ── "Ano N" / "Ano III" ───────────────────────────────────────────────────
    {
        static const QRegularExpression reAnoN(QStringLiteral("\\bano\\s+(\\d+)"));
        const auto mN = reAnoN.match(s);
        if (mN.hasMatch()) { days += mN.captured(1).toDouble() * 372.0; ok = true; }
        else {
            static const QRegularExpression reAnoR(QStringLiteral("\\bano\\s+([ivxlcdm]+)\\b"));
            const auto mR = reAnoR.match(s);
            if (mR.hasMatch()) {
                const int r = romanToInt(mR.captured(1));
                if (r > 0) { days += r * 372.0; ok = true; }
            }
        }
    }

    // ── estações (aprox. por dia do ano) ──────────────────────────────────────
    if      (s.contains(QStringLiteral("primavera"))) { days += 80.0;  ok = true; }
    else if (s.contains(QStringLiteral("verão")) ||
             s.contains(QStringLiteral("verao")))     { days += 172.0; ok = true; }
    else if (s.contains(QStringLiteral("outono")))    { days += 266.0; ok = true; }
    else if (s.contains(QStringLiteral("inverno")))   { days += 355.0; ok = true; }

    // ── nudges relativos fracos ───────────────────────────────────────────────
    if (s.contains(QStringLiteral("antes")))  { days -= 0.5; ok = true; }
    if (s.contains(QStringLiteral("depois"))) { days += 0.5; ok = true; }

    if (okOut) *okOut = ok;
    return days * DAY + minutes;
}

} // namespace TimelineChrono
