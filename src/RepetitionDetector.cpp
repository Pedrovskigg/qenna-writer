#include "RepetitionDetector.h"

#include <QHash>
#include <QRegularExpression>

namespace {

// Palavras funcionais: repetem por obrigação gramatical, não por descuido de
// quem escreve. Sem esta lista, "de" e "que" afogariam qualquer resultado útil.
// Não pretende ser exaustiva — pretende cobrir o que aparece o tempo todo.
const char* const kStopPt[] = {
    "a","o","as","os","um","uma","uns","umas","de","do","da","dos","das","em","no","na",
    "nos","nas","por","pelo","pela","pelos","pelas","para","pra","com","sem","sob","sobre",
    "entre","até","desde","após","ante","contra","que","quem","qual","quais","quando",
    "onde","como","porque","pois","mas","porém","contudo","todavia","entretanto","ou",
    "nem","se","já","ainda","mais","menos","muito","muita","muitos","muitas","pouco",
    "pouca","poucos","poucas","tão","tanto","tanta","todo","toda","todos","todas","outro",
    "outra","outros","outras","mesmo","mesma","mesmos","mesmas","este","esta","estes",
    "estas","esse","essa","esses","essas","aquele","aquela","aqueles","aquelas","isto",
    "isso","aquilo","eu","tu","ele","ela","nós","vós","eles","elas","você","vocês","me",
    "te","se","lhe","nos","vos","lhes","meu","minha","meus","minhas","teu","tua","teus",
    "tuas","seu","sua","seus","suas","nosso","nossa","nossos","nossas","dele","dela",
    "deles","delas","ser","estar","ter","haver","era","eram","foi","foram","seja","sejam",
    "está","estão","estava","estavam","tem","têm","tinha","tinham","havia","não","sim",
    "também","só","apenas","então","agora","depois","antes","aqui","ali","lá","cá","assim",
    "bem","mal","talvez","quase","sempre","nunca","jamais","onde","enquanto","durante",
    // Indefinidos e advérbios de lugar: gramaticalmente obrigatórios, repetem
    // sem que ninguém perceba. Entraram depois do primeiro teste real, em que
    // "alguma" e "atrás" apareceram grifados sem incomodar ninguém.
    "algum","alguma","alguns","algumas","alguém","algo","nada","tudo","cada","qualquer",
    "atrás","frente","lado","dentro","fora","cima","baixo","perto","longe","junto",
    "outro","outra","ainda","apenas","inclusive","sobretudo",
};
const char* const kStopEn[] = {
    "the","a","an","and","or","but","if","then","than","that","this","these","those","of",
    "in","on","at","to","for","with","without","from","by","about","into","over","under",
    "between","through","during","before","after","above","below","up","down","out","off",
    "again","further","is","are","was","were","be","been","being","have","has","had",
    "having","do","does","did","doing","will","would","shall","should","can","could","may",
    "might","must","i","you","he","she","it","we","they","me","him","her","us","them","my",
    "your","his","its","our","their","mine","yours","hers","ours","theirs","who","whom",
    "which","what","when","where","why","how","all","any","both","each","few","more","most",
    "other","some","such","no","nor","not","only","own","same","so","too","very","just",
    "now","here","there","also","still","yet","ever","never","always",
};
const char* const kStopEs[] = {
    "el","la","los","las","un","una","unos","unas","de","del","al","en","por","para","con",
    "sin","sobre","entre","hasta","desde","ante","contra","que","quien","quienes","cual",
    "cuales","cuando","donde","como","porque","pues","pero","sino","aunque","o","u","ni",
    "si","ya","aún","más","menos","mucho","mucha","muchos","muchas","poco","poca","pocos",
    "pocas","tan","tanto","tanta","todo","toda","todos","todas","otro","otra","otros",
    "otras","mismo","misma","este","esta","estos","estas","ese","esa","esos","esas","aquel",
    "aquella","esto","eso","aquello","yo","tú","él","ella","nosotros","vosotros","ellos",
    "ellas","usted","ustedes","me","te","se","le","nos","os","les","mi","mis","tu","tus",
    "su","sus","nuestro","nuestra","ser","estar","tener","haber","era","eran","fue","fueron",
    "sea","sean","está","están","estaba","estaban","tiene","tienen","tenía","había","no",
    "sí","también","solo","entonces","ahora","después","antes","aquí","allí","así","bien",
    "mal","quizás","casi","siempre","nunca","jamás","mientras","durante",
};

// Sem acento e em minúsculas, pra comparar "papéis" com "papel".
QString simplify(const QString& w)
{
    const QString d = w.toLower().normalized(QString::NormalizationForm_D);
    QString out;
    out.reserve(d.size());
    for (const QChar& c : d)
        if (c.category() != QChar::Mark_NonSpacing) out.append(c);
    return out;
}

// Advérbio de modo: -mente em pt/es, -ly em inglês. O tamanho mínimo evita
// pegar o substantivo "mente" e companhia — advérbio de modo é palavra longa.
bool ehAdverbioDeModo(const QString& lower, const QString& lang)
{
    if (lang == QLatin1String("en"))
        return lower.size() >= 6 && lower.endsWith(QLatin1String("ly"));
    return lower.size() >= 8 && lower.endsWith(QLatin1String("mente"));
}

QSet<QString> makeSet(const char* const* words, int count)
{
    QSet<QString> out;
    out.reserve(count);
    for (int i = 0; i < count; ++i) out.insert(QString::fromUtf8(words[i]));
    return out;
}

// Palavra = sequência de letras, aceitando acento, hífen interno e apóstrofe
// ("mão", "meio-dia", "d'água"). \w não serve: pega dígito e underscore.
const QRegularExpression& wordRe()
{
    // A aspa curva usa a sintaxe \x{...} do PCRE, e dentro de classe o
    // hifen precisa vir escapado pra nao virar range. Padrao invalido aqui
    // nao explode: o Qt devolve zero resultados, calado.
    static const QRegularExpression re(
        QStringLiteral("[\\p{L}]+(?:['\\x{2019}\\-][\\p{L}]+)*"),
        QRegularExpression::UseUnicodePropertiesOption);
    return re;
}

} // namespace

void RepetitionDetector::setLanguage(const QString& langCode)
{
    const QString lang = langCode.left(2).toLower();
    m_lang = lang;
    if (lang == QLatin1String("en"))
        m_stopWords = makeSet(kStopEn, int(std::size(kStopEn)));
    else if (lang == QLatin1String("es"))
        m_stopWords = makeSet(kStopEs, int(std::size(kStopEs)));
    else
        m_stopWords = makeSet(kStopPt, int(std::size(kStopPt)));
}

bool RepetitionDetector::isIgnored(const QString& lowerWord) const
{
    if (lowerWord.size() < kMinWordLength) return true;
    if (m_stopWords.contains(lowerWord)) return true;
    if (m_ignored.contains(lowerWord)) return true;
    return false;
}

QVector<RepetitionDetector::Group> RepetitionDetector::analyze(const QString& plainText) const
{
    QVector<Group> resultado;
    if (plainText.isEmpty()) return resultado;

    struct Token { int pos; int len; QString word; QString key; int index; };
    QVector<Token> tokens;
    // Índice em PALAVRAS (todas, inclusive as ignoradas): a distância que o
    // leitor percebe é medida em palavras lidas, não em caracteres — um trecho
    // com palavras longas não "afasta" a repetição.
    int wordIndex = 0;

    auto it = wordRe().globalMatch(plainText);
    while (it.hasNext()) {
        const auto m = it.next();
        const QString bruto = m.captured();
        const QString lower = bruto.toLower();
        const int idxAtual = wordIndex++;
        if (isIgnored(lower)) continue;

        // A chave é a própria palavra, sem acento pra "papéis" não escapar de
        // "papeis" digitado sem acento — mas SEM reduzir a raiz: flexão
        // diferente é palavra diferente aqui, por decisão do usuário.
        tokens.append({ int(m.capturedStart()), int(m.capturedLength()), bruto,
                        simplify(lower), idxAtual });
    }
    if (tokens.size() < 2) return resultado;

    QHash<QString, QVector<int>> porChave;   // chave -> índices em `tokens`
    for (int i = 0; i < tokens.size(); ++i) porChave[tokens[i].key].append(i);

    for (auto kv = porChave.constBegin(); kv != porChave.constEnd(); ++kv) {
        const QVector<int>& idxs = kv.value();
        if (idxs.size() < 2) continue;

        // Só interessa quando DUAS ocorrências caem perto uma da outra. Uma
        // palavra usada 8 vezes ao longo de um capítulo inteiro não é defeito.
        int menorGap = INT_MAX;
        QSet<int> proximas;
        for (int i = 1; i < idxs.size(); ++i) {
            const int gap = tokens[idxs[i]].index - tokens[idxs[i - 1]].index;
            if (gap <= m_proximity) {
                proximas.insert(idxs[i - 1]);
                proximas.insert(idxs[i]);
                menorGap = qMin(menorGap, gap);
            }
        }
        if (proximas.isEmpty()) continue;

        Group g;
        g.kind = SameWord;
        g.stem = kv.key();
        g.closestGap = menorGap;
        QVector<int> ordenadas(proximas.constBegin(), proximas.constEnd());
        std::sort(ordenadas.begin(), ordenadas.end());

        QHash<QString, int> frequencia;
        int anterior = -1;
        for (int i : ordenadas) {
            const Token& t = tokens[i];
            const int desde = (anterior < 0) ? 0 : (t.index - tokens[anterior].index);
            g.hits.append({ t.pos, t.len, t.word, desde });
            frequencia[t.word.toLower()]++;
            anterior = i;
        }
        // Rótulo = a forma mais usada entre as ocorrências próximas; é o que o
        // autor reconhece ao bater o olho na lista.
        int melhor = 0;
        for (auto f = frequencia.constBegin(); f != frequencia.constEnd(); ++f) {
            if (f.value() > melhor) { melhor = f.value(); g.label = f.key(); }
        }
        resultado.append(g);
    }

    // ── Acúmulo de advérbios em -mente ──
    // Aqui as palavras são DIFERENTES entre si ("rapidamente", "lentamente") —
    // o defeito é a terminação voltando, não a palavra. Por isso nenhuma busca
    // por repetição literal pega isso, e por isso escapa tanto.
    {
        QVector<int> adverbios;
        for (int i = 0; i < tokens.size(); ++i) {
            if (ehAdverbioDeModo(tokens[i].word.toLower(), m_lang)) adverbios.append(i);
        }
        // Janela dobrada: o vício se mede por parágrafo, não por frase. Dois
        // "-mente" na mesma frase são raros; o que empesta é o punhado deles
        // espalhado pelo mesmo fôlego de leitura.
        const int janela = m_proximity * 2;
        int i = 0;
        while (i < adverbios.size()) {
            int j = i;
            while (j + 1 < adverbios.size()
                   && tokens[adverbios[j + 1]].index - tokens[adverbios[j]].index <= janela) {
                ++j;
            }
            if (j > i) {   // dois ou mais no mesmo trecho
                Group g;
                g.kind = AdverbPileup;
                g.stem = QStringLiteral("-mente");
                g.label = QStringLiteral("-mente");
                g.closestGap = INT_MAX;
                int anterior = -1;
                for (int k = i; k <= j; ++k) {
                    const Token& t = tokens[adverbios[k]];
                    const int desde = (anterior < 0) ? 0 : (t.index - tokens[anterior].index);
                    if (desde > 0) g.closestGap = qMin(g.closestGap, desde);
                    g.hits.append({ t.pos, t.len, t.word, desde });
                    anterior = adverbios[k];
                }
                resultado.append(g);
            }
            i = j + 1;
        }
    }

    // Mais apertado primeiro: é o que mais salta ao ouvido na leitura.
    std::sort(resultado.begin(), resultado.end(), [](const Group& a, const Group& b) {
        if (a.closestGap != b.closestGap) return a.closestGap < b.closestGap;
        if (a.hits.size() != b.hits.size()) return a.hits.size() > b.hits.size();
        return a.label < b.label;
    });
    return resultado;
}
