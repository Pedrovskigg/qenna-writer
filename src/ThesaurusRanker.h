#pragma once

#include "Thesaurus.h"

#include <QList>
#include <QString>
#include <QStringList>

// Ordena as acepções e os sinônimos de uma consulta ao dicionário usando o
// TEXTO DO PRÓPRIO AUTOR como referência.
//
// O problema: pedir sinônimo de "casa" em "não estava em casa" devolve 21
// sentidos — armazém, classe, botoeira, estirpe — e só um serve. O dicionário
// não lê a frase, só a palavra.
//
// A saída, sem IA e sem análise gramatical: o app não sabe o que "casa"
// significa ali, mas sabe quais palavras andam juntas no manuscrito. Se os
// sinônimos de uma acepção costumam aparecer perto das palavras que estão em
// volta do cursor, aquela acepção é a provável. É desambiguação por vizinhança
// (a ideia do algoritmo de Lesk), alimentada pelo corpus do próprio livro em vez
// de um corpus genérico — e por isso fica melhor conforme o autor escreve.
class ThesaurusRanker {
public:
    struct RankedSense {
        Thesaurus::Sense sense;
        double score = 0.0;
        // Alguma evidência real no texto do autor sustentou esta posição? Sem
        // isso a ordem é só o palpite morno do dicionário, e a UI não deve
        // fingir confiança que não tem.
        bool grounded = false;
    };

    // corpus: texto plano do manuscrito (quanto maior, melhor a ordenação).
    // context: o parágrafo (ou trecho) em volta da palavra consultada.
    // Devolve as acepções da melhor pra pior, com os sinônimos de cada uma
    // também reordenados.
    static QList<RankedSense> rank(const QList<Thesaurus::Sense>& senses,
                                   const QString& word,
                                   const QString& context,
                                   const QString& corpus);

    // Palavras "de conteúdo" de um trecho: minúsculas, sem pontuação, sem
    // palavras funcionais e sem as muito curtas. Exposto porque a UI usa a mesma
    // regra pra decidir se há contexto suficiente.
    static QStringList contentWords(const QString& text);

private:
    static bool isStopWord(const QString& w);
};
