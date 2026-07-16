#pragma once

#include "ElementsStore.h"

#include <QRegularExpression>
#include <QString>
#include <QVector>

// Regex de atribuição (nome aparece no texto após o travessão/aspas de
// fechamento) e de proximidade (verbo de fala perto do nome, sem tag
// explícita) para um personagem — uma linha bate em no máximo um desses
// modos por vez, dependendo se a fala tem atribuição textual ou não.
struct DialogueScannerToken {
    QString elementId;
    QRegularExpression attrRe;
    QRegularExpression proximityRe;
};

// Uma fala com atribuição confiante (exatamente 1 personagem casou).
struct DetectedDialogueLine {
    QString text;
    QString characterId;
};

// Motor de atribuição de diálogo — porta scanDialogAttribution/
// buildScannerTokens do Mira 1 (src/App.jsx, src/dialogVerbs.js) para C++.
// Funções estáticas, sem estado, mesmo molde de CharacterDetector.
class DialogueDetector {
public:
    // Um token por personagem: nome completo + primeiro nome (só se único
    // entre os personagens) + cada apelido/alias, todos como frase exata
    // (ao contrário de CharacterDetector::buildTokens, que quebra em
    // palavras — aqui precisamos do nome inteiro pra montar as regexes de
    // atribuição/proximidade).
    static QVector<DialogueScannerToken> buildScannerTokens(const QList<Element>& characters);

    // Varre plainText (parágrafos separados por '\n', como
    // QTextEdit::toPlainText() do documento inteiro produz — diferente de
    // QTextCursor::selectedText(), que usa U+2029) e retorna só as falas com
    // atribuição confiante. Parágrafos ambíguos (0 ou >1 personagem bateu) são
    // descartados aqui — quem chama nunca vê a incerteza.
    // narrator pode ser nullptr (projeto sem narrador marcado).
    static QVector<DetectedDialogueLine> scanConfidentDialogues(
        const QString& plainText,
        const QVector<DialogueScannerToken>& tokens,
        const Element* narrator);

private:
    // Tenta atribuir uma única linha; retorna elementId ou QString() se
    // ambíguo/sem casamento. Espelha scanDialogAttribution do Mira 1.
    static QString attributeLine(const QString& text,
                                  const QVector<DialogueScannerToken>& tokens,
                                  const Element* narrator);
};
