#pragma once

#include <QSet>
#include <QString>

// Agrupamento de papéis (roles) em grupos de importância. Define quem ganha
// trilha de linha do tempo automaticamente.
//
//  Principal  → trilha automática
//  Secundario → questionado (pergunta uma vez, grava a resposta em Element.trackMode)
//  Terciario  → esquecido (só manual)
//  None       → role vazia: não acompanha
//
// Role personalizada (digitada pelo usuário, fora da lista) é tratada como
// Principal — pela lógica: ninguém digita um papel à mão para um figurante.
namespace RoleTiers {

enum class Tier { Principal, Secundario, Terciario, None };

inline Tier tierFor(const QString& roleRaw)
{
    const QString r = roleRaw.trimmed().toUpper();
    if (r.isEmpty()) return Tier::None;

    static const QSet<QString> principal = {
        QStringLiteral("PROTAGONISTA"),
        QStringLiteral("ANTAGONISTA"),
        QStringLiteral("DEUTERAGONISTA"),
    };
    static const QSet<QString> secundario = {
        QStringLiteral("COADJUVANTE"),
        QStringLiteral("CONTRAPONTO"),
        QStringLiteral("TRICKSTER"),
        QStringLiteral("MENTOR"),
    };
    static const QSet<QString> terciario = {
        QStringLiteral("FIGURANTE"),
    };

    if (principal.contains(r))  return Tier::Principal;
    if (secundario.contains(r)) return Tier::Secundario;
    if (terciario.contains(r))  return Tier::Terciario;
    return Tier::Principal; // role custom → auto
}

// Decisão final, já considerando o override manual (Element::trackMode).
//  trackMode "on"  → sempre ganha trilha
//  trackMode "off" → nunca
//  ""              → segue o tier (Principal=auto, Secundario=perguntar, resto=não)
// Retorna: 1 = sim, 0 = não, -1 = perguntar
inline int autoTrackDecision(const QString& role, const QString& trackMode)
{
    if (trackMode == QStringLiteral("on"))  return 1;
    if (trackMode == QStringLiteral("off")) return 0;
    switch (tierFor(role)) {
    case Tier::Principal:  return 1;
    case Tier::Secundario: return -1;
    default:               return 0;
    }
}

} // namespace RoleTiers
