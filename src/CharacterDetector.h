#pragma once

#include "ElementsStore.h"

#include <QList>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>

struct CharacterDetection {
    QString elementId;
    QString docKey;
};

struct ScanResult {
    QList<CharacterDetection> autoMark;   // narrador: marca calado
    QList<CharacterDetection> announce;   // o resto: marca e avisa no DocHeader
};

// Os gatilhos da detecção de presença de personagem. Funções estáticas, sem
// estado. O scan em si roda no MainWindow, em lote, no event loop.
class CharacterDetector {
public:
    // Mínimo de ocorrências para considerar presença.
    static constexpr int kThreshold = 3;

    // Mínimo de palavras no documento para auto-marcar narrador.
    static constexpr int kNarratorMinWords = 30;

    // Pedaços de nome que dois ou mais personagens têm (o sobrenome da
    // família): sozinhos não denunciam ninguém. Minúsculas.
    static QSet<QString> sharedNameParts(const QList<Element>& elements);

    // As palavras que denunciam o personagem no texto (minúsculas): cada
    // pedaço do nome com 3+ letras, menos "de/da/das/dos…", algarismo romano
    // ("III") e sobrenome compartilhado (fora o primeiro nome, que sempre
    // vale); o nome inteiro também, quando tem mais de uma palavra; e os
    // apelidos.
    static QStringList buildTokens(const Element& e, const QSet<QString>& sharedParts = {});

    // O regex de um gatilho: palavra inteira, sem diferença de maiúscula, e
    // com letra acentuada contando como letra (sem isso José, André e Ângela
    // nunca batiam: o \b do PCRE é só ASCII).
    static QRegularExpression tokenRegex(const QString& token);

    // Quantas vezes o gatilho aparece COMO NOME no texto (o original, não o
    // minúsculo): só conta ocorrência com inicial maiúscula, pra "a água
    // clara" não virar a Ana Clara. Para de contar em `limit`.
    static int countNamed(const QRegularExpression& re, const QString& text, int limit);
};
