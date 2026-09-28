#pragma once
// Ajudantes da Mesa de preenchimento da Timeline (o antigo Gerador de
// Timeline): o que o Qenna entendeu de um marcador ("leitura ao vivo"),
// "+1 dia" a partir do marcador anterior e a sugestão tirada da primeira
// frase do capítulo. Tudo local, sem IA — regex, como o detector de
// repetições. Nada aqui mexe no modelo.

#include <QString>

namespace FillAssist {

// ── Leitura ao vivo ──────────────────────────────────────────────────────────
struct Reading {
    enum Kind { Empty, Ok, Bad };
    Kind    kind = Empty;
    QString text;            // "→ dia 9", "→ 20 anos antes · Flashback"…
    QString compact;         // mesma coisa mais curta, pra coluna estreita
};
// startOk/startChrono: data-base do manuscrito (decide o Flashback, igual ao
// syncStoryTimeline).
Reading reading(const QString& marker, bool startOk, qreal startChrono);

// ── "+N dias" a partir de outro marcador ─────────────────────────────────────
enum class Unit { Day, Week, Month, Year };
// "Dia 4" + 1 dia → "Dia 5"; "15/05/2026" + 1 mês → "15/06/2026". Vazio se
// o marcador não tem uma forma que dê pra somar (ex.: "Verão de 1999").
QString shifted(const QString& marker, int amount, Unit unit);

// ── Sugestão pela primeira frase ─────────────────────────────────────────────
struct Suggestion {
    QString marker;          // "Dia 9"
    QString why;             // "“cinco dias depois” e o Cap 5 é Dia 4"
    QString phrase;          // o trecho do texto que gerou a sugestão ("Cinco dias depois")
    bool valid() const { return !marker.isEmpty(); }
};
// text = texto do capítulo/cena (só a primeira frase é lida); prevMarker =
// marcador efetivo da coluna anterior; prevLabel = "Cap 5" (pro "porquê").
Suggestion suggest(const QString& text, const QString& prevMarker, const QString& prevLabel);

QString firstSentence(const QString& text);
// Começo do capítulo pra mostrar: frases inteiras até maxChars.
QString opening(const QString& text, int maxChars = 320);

} // namespace FillAssist
