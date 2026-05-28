#pragma once

#include "ElementsStore.h"

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

struct CharacterDetection {
    QString elementId;
    QString docKey;
};

struct ScanResult {
    QList<CharacterDetection> autoMark;
    QList<CharacterDetection> confirm;
};

// Lógica de detecção de presença de personagem. Funções estáticas — sem estado,
// thread-safe, pode rodar em qualquer thread via QtConcurrent::run.
class CharacterDetector {
public:
    // Mínimo de ocorrências para considerar presença.
    static constexpr int kThreshold = 3;

    // Mínimo de palavras no documento para auto-marcar narrador.
    static constexpr int kNarratorMinWords = 30;

    static ScanResult scan(const QString& plainText,
                           const QString& docKey,
                           const QList<Element>& elements,
                           const QSet<QString>& alreadyPresent,
                           const QSet<QString>& autoIds,
                           const QSet<QString>& neverIds,
                           const QSet<QString>& rejectedKeys,
                           bool markAll);

    static QStringList buildTokensPublic(const Element& e) { return buildTokens(e); }

private:
    static QStringList buildTokens(const Element& e);
};
