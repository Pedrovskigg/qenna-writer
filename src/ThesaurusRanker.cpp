#include "ThesaurusRanker.h"

#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <cmath>

namespace {

// Palavras funcionais do português: aparecem em todo parágrafo e por isso não
// dizem nada sobre o assunto. Sem cortá-las, "de/que/em" casariam com o
// manuscrito inteiro e a pontuação viraria ruído.
const char* const kStopWords[] = {
    "a","à","às","ao","aos","agora","ainda","algum","alguma","alguns","ante","antes",
    "aquela","aquele","aquilo","as","até","com","como","da","das","de","dela","dele",
    "delas","deles","depois","do","dos","e","é","ela","elas","ele","eles","em","entre",
    "era","eram","essa","essas","esse","esses","esta","estas","este","estes","eu","foi",
    "foram","há","isso","isto","já","lhe","lhes","mais","mas","me","mesmo","meu","minha",
    "muito","na","nas","não","no","nos","nós","num","numa","o","os","ou","para","pela",
    "pelas","pelo","pelos","por","porque","quando","que","quem","se","sem","ser","seu",
    "sua","seus","suas","só","também","te","tem","têm","tinha","tu","um","uma","uns",
    "umas","vez","você","vocês","lá","aqui","ali","onde","muito","pouco","tudo","todo",
    "toda","todos","todas","outro","outra","ser","estar","ter","fazer","dizer","ir",
};

QSet<QString> makeStopSet()
{
    QSet<QString> s;
    for (const char* w : kStopWords) s.insert(QString::fromUtf8(w));
    return s;
}

const QRegularExpression& wordSplitter()
{
    // Divide por qualquer coisa que não seja letra/número — mantém acentos.
    static const QRegularExpression re(QStringLiteral("[^\\p{L}\\p{N}]+"),
                                       QRegularExpression::UseUnicodePropertiesOption);
    return re;
}

} // namespace

bool ThesaurusRanker::isStopWord(const QString& w)
{
    static const QSet<QString> kSet = makeStopSet();
    return kSet.contains(w);
}

QStringList ThesaurusRanker::contentWords(const QString& text)
{
    QStringList out;
    const QStringList raw = text.toLower().split(wordSplitter(), Qt::SkipEmptyParts);
    for (const QString& w : raw) {
        if (w.size() < 4) continue;      // "casa" tem 4; menor que isso é ruído
        if (isStopWord(w)) continue;
        out << w;
    }
    return out;
}

QList<ThesaurusRanker::RankedSense> ThesaurusRanker::rank(const QList<Thesaurus::Sense>& senses,
                                                          const QString& word,
                                                          const QString& context,
                                                          const QString& corpus)
{
    QList<RankedSense> out;
    out.reserve(senses.size());
    for (const Thesaurus::Sense& s : senses) out.append({ s, 0.0, false });
    if (senses.isEmpty()) return out;

    // Palavras em volta do cursor, sem a própria palavra consultada.
    QSet<QString> ctx;
    for (const QString& w : contentWords(context))
        if (w.compare(word, Qt::CaseInsensitive) != 0) ctx.insert(w);

    // Frequência de cada palavra no manuscrito inteiro. Serve pra duas coisas:
    // saber se o autor já usa aquele sinônimo (camada 2) e descartar arcaísmo.
    QHash<QString, int> corpusFreq;
    // Parágrafos do corpus que tocam o contexto atual — é neles que a
    // vizinhança de cada acepção é medida.
    QList<QSet<QString>> nearbyParagraphs;

    if (!corpus.isEmpty()) {
        const QStringList paras = corpus.split(QRegularExpression(QStringLiteral("\\n+")),
                                               Qt::SkipEmptyParts);
        for (const QString& p : paras) {
            const QStringList words = contentWords(p);
            if (words.isEmpty()) continue;

            QSet<QString> bag;
            for (const QString& w : words) {
                corpusFreq[w] += 1;
                bag.insert(w);
            }
            if (ctx.isEmpty()) continue;
            // Interseção com o contexto: parágrafo que fala do mesmo assunto.
            for (const QString& c : ctx) {
                if (bag.contains(c)) { nearbyParagraphs.append(bag); break; }
            }
        }
    }

    for (RankedSense& rs : out) {
        double score = 0.0;
        bool grounded = false;

        for (const QString& synRaw : rs.sense.synonyms) {
            const QString syn = synRaw.toLower().trimmed();
            if (syn.isEmpty() || syn.contains(QLatin1Char(' '))) continue;

            // (a) O sinônimo aparece no próprio contexto: sinal fortíssimo de
            // que é este o sentido em jogo.
            if (ctx.contains(syn)) { score += 12.0; grounded = true; }

            // (b) O sinônimo aparece perto das palavras do contexto, em outros
            // trechos do livro. É o coração da desambiguação.
            int near = 0;
            for (const QSet<QString>& bag : nearbyParagraphs)
                if (bag.contains(syn)) ++near;
            if (near > 0) {
                score += 4.0 * std::log(1.0 + near);
                grounded = true;
            }

            // (c) O autor já usa essa palavra em algum lugar: empurra o
            // vocabulário dele pra frente e o arcaísmo pra trás. Peso baixo de
            // propósito — é desempate, não argumento.
            const int freq = corpusFreq.value(syn, 0);
            if (freq > 0) {
                score += std::min(2.0, 0.4 * std::log(1.0 + freq));
                grounded = true;
            }
        }

        // Normaliza pelo tamanho: acepção com 30 sinônimos não pode ganhar só
        // por ter mais bilhetes na rifa.
        const int n = qMax(1, rs.sense.synonyms.size());
        rs.score = score / std::sqrt(double(n));
        rs.grounded = grounded;
    }

    std::stable_sort(out.begin(), out.end(), [](const RankedSense& a, const RankedSense& b) {
        return a.score > b.score;
    });

    // Dentro de cada acepção: o que o autor já escreveu vem primeiro, depois o
    // que aparece no contexto, e o resto mantém a ordem do dicionário.
    for (RankedSense& rs : out) {
        QStringList syns = rs.sense.synonyms;
        std::stable_sort(syns.begin(), syns.end(),
                         [&](const QString& a, const QString& b) {
            const QString la = a.toLower(), lb = b.toLower();
            const int fa = corpusFreq.value(la, 0), fb = corpusFreq.value(lb, 0);
            const bool ca = ctx.contains(la), cb = ctx.contains(lb);
            if (ca != cb) return ca;      // presente na frase primeiro
            if ((fa > 0) != (fb > 0)) return fa > 0;  // já usado pelo autor
            return fa > fb;
        });
        rs.sense.synonyms = syns;
    }

    return out;
}
