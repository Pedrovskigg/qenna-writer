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
    "isso","aquilo","me",
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
    "might","must","it","me","him","her","us","them","my",
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
    "aquella","esto","eso","aquello",
    "me","te","se","le","nos","os","les","mi","mis","tu","tus",
    "su","sus","nuestro","nuestra","ser","estar","tener","haber","era","eran","fue","fueron",
    "sea","sean","está","están","estaba","estaban","tiene","tienen","tenía","había","no",
    "sí","también","solo","entonces","ahora","después","antes","aquí","allí","así","bien",
    "mal","quizás","casi","siempre","nunca","jamás","mientras","durante",
};
// Italiano e francês: artigo e preposição elididos ("l'uomo", "d'une") são
// separados da palavra antes de chegar aqui — ver elidedPrefixLength().
const char* const kStopIt[] = {
    "il","lo","la","i","gli","le","un","uno","una","di","del","dello","della","dei","degli",
    "delle","a","al","allo","alla","ai","agli","alle","da","dal","dallo","dalla","dai",
    "dagli","dalle","in","nel","nello","nella","nei","negli","nelle","su","sul","sullo",
    "sulla","sui","sugli","sulle","con","per","tra","fra","senza","sotto","sopra","dopo",
    "prima","verso","contro","che","chi","cui","quale","quali","quando","dove","come",
    "perché","poiché","ma","però","tuttavia","eppure","o","oppure","né","se","già","ancora",
    "più","meno","molto","molta","molti","molte","poco","poca","pochi","poche","tanto",
    "tanta","tanti","tante","tutto","tutta","tutti","tutte","altro","altra","altri","altre",
    "stesso","stessa","stessi","stesse","questo","questa","questi","queste","quello",
    "quella","quelli","quelle","ciò","mi","ti","si","ci","vi","gli","ne","lo","mio","mia",
    "miei","mie","tuo","tua","tuoi","tue","suo","sua","suoi","sue","nostro","nostra",
    "vostro","vostra","loro","essere","avere","stare","era","erano","fu","furono","sia",
    "siano","è","sono","stava","stavano","ha","hanno","aveva","avevano","non","sì","anche",
    "solo","soltanto","allora","ora","adesso","poi","qui","qua","lì","là","così","bene",
    "male","forse","quasi","sempre","mai","mentre","durante","qualche","qualcuno",
    "qualcosa","niente","nulla","ogni","qualsiasi","dietro","davanti","dentro","fuori",
    "vicino","lontano","insieme",
};
const char* const kStopFr[] = {
    "le","la","les","un","une","des","de","du","au","aux","en","dans","par","pour","avec",
    "sans","sous","sur","entre","jusque","depuis","après","avant","devant","derrière",
    "contre","vers","chez","que","qui","quoi","dont","quel","quelle","quels","quelles",
    "quand","où","comme","parce","car","mais","pourtant","cependant","ou","ni","si","déjà",
    "encore","plus","moins","très","trop","peu","beaucoup","tant","tout","toute","tous",
    "toutes","autre","autres","même","mêmes","ce","cet","cette","ces","cela","ça","ceci",
    "celui","celle","ceux","celles","me","te","se","lui","nous","vous","leur","leurs","mon",
    "ma","mes","ton","ta","tes","son","sa","ses","notre","nos","votre","vos","être","avoir",
    "était","étaient","fut","furent","soit","soient","est","sont","avait","avaient","a",
    "ont","ne","pas","non","oui","aussi","seulement","alors","maintenant","puis","ensuite",
    "ici","là","ainsi","bien","mal","peut-être","presque","toujours","jamais","pendant",
    "tandis","quelque","quelques","quelqu'un","rien","chaque","aucun",
    "aucune","dedans","dehors","près","loin","ensemble","y",
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

// Pronomes pessoais do caso reto, pela norma culta. Saíram das palavras
// funcionais: repetir "ela... ela... ela" é vício que o leitor sente, ao
// contrário de "de" e "que". Janela própria e curta (kPronounProximity).
// Inglês fica sem "it": o português omite o sujeito neutro, então não há
// vício equivalente a imitar — e "it" repete por pura mecânica da língua.
const char* const kPronPt[] = {
    "eu","tu","ele","ela","nós","vós","eles","elas","você","vocês",
};
const char* const kPronEn[] = {
    "i","you","he","she","we","they",
};
const char* const kPronEs[] = {
    "yo","tú","él","ella","nosotros","vosotros","ellos","ellas","usted","ustedes",
};
const char* const kPronIt[] = {
    "io","tu","lui","lei","noi","voi","loro","egli","ella","essi","esse",
};
// "il/elle" repetido é o vício; "on" entra porque narração em francês abusa dele.
const char* const kPronFr[] = {
    "je","tu","il","elle","nous","vous","ils","elles","on",
};

// Elisão do italiano e do francês: "l'uomo", "dell'anima", "d'une", "qu'il".
// Devolve quantos caracteres pular pra chegar na palavra de verdade — sem
// isso "l'homme" e "homme" seriam palavras diferentes e a repetição escaparia.
int elidedPrefixLength(const QString& lowerWord)
{
    for (int i = 0; i < lowerWord.size() && i <= 5; ++i) {
        const QChar c = lowerWord.at(i);
        if (c == QLatin1Char('\'') || c == QChar(0x2019))
            return (i + 1 < lowerWord.size()) ? i + 1 : 0;
    }
    return 0;
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
    if (lang == QLatin1String("en")) {
        m_stopWords = makeSet(kStopEn, int(std::size(kStopEn)));
        m_pronouns  = makeSet(kPronEn, int(std::size(kPronEn)));
    } else if (lang == QLatin1String("es")) {
        m_stopWords = makeSet(kStopEs, int(std::size(kStopEs)));
        m_pronouns  = makeSet(kPronEs, int(std::size(kPronEs)));
    } else if (lang == QLatin1String("it")) {
        m_stopWords = makeSet(kStopIt, int(std::size(kStopIt)));
        m_pronouns  = makeSet(kPronIt, int(std::size(kPronIt)));
    } else if (lang == QLatin1String("fr")) {
        m_stopWords = makeSet(kStopFr, int(std::size(kStopFr)));
        m_pronouns  = makeSet(kPronFr, int(std::size(kPronFr)));
    } else {
        m_stopWords = makeSet(kStopPt, int(std::size(kStopPt)));
        m_pronouns  = makeSet(kPronPt, int(std::size(kPronPt)));
    }
}

bool RepetitionDetector::isIgnored(const QString& lowerWord) const
{
    // Pronome vem antes do tamanho mínimo: "ele", "ela", "he" e "we" são
    // curtos e seriam descartados justamente sendo o vício que se quer ver.
    if (isPronoun(lowerWord)) return m_ignored.contains(lowerWord);
    if (lowerWord.size() < kMinWordLength) return true;
    if (m_stopWords.contains(lowerWord)) return true;
    if (m_ignored.contains(lowerWord)) return true;
    return false;
}

QVector<RepetitionDetector::Group> RepetitionDetector::analyze(const QString& plainText) const
{
    QVector<Group> resultado;
    if (plainText.isEmpty()) return resultado;

    struct Token { int pos; int len; QString word; QString key; int index; int paragraph; };
    QVector<Token> tokens;
    // Índice em PALAVRAS (todas, inclusive as ignoradas): a distância que o
    // leitor percebe é medida em palavras lidas, não em caracteres — um trecho
    // com palavras longas não "afasta" a repetição.
    int wordIndex = 0;
    // Parágrafo de cada palavra. toPlainText() do documento separa blocos com
    // '\n'; selectedText() usaria U+2029 — aceitar os dois.
    int paragrafo = 0;
    int varridoAte = 0;

    auto it = wordRe().globalMatch(plainText);
    while (it.hasNext()) {
        const auto m = it.next();
        const int inicio = int(m.capturedStart());
        for (int c = varridoAte; c < inicio; ++c) {
            const QChar ch = plainText.at(c);
            if (ch == QLatin1Char('\n') || ch == QChar::ParagraphSeparator) ++paragrafo;
        }
        varridoAte = int(m.capturedEnd());

        QString bruto = m.captured();
        int pos = inicio;
        const int idxAtual = wordIndex++;
        if (m_lang == QLatin1String("it") || m_lang == QLatin1String("fr")) {
            const int corte = elidedPrefixLength(bruto.toLower());
            bruto = bruto.mid(corte);
            pos += corte;
        }
        const QString lower = bruto.toLower();
        if (isIgnored(lower)) continue;

        // A chave é a própria palavra, sem acento pra "papéis" não escapar de
        // "papeis" digitado sem acento — mas SEM reduzir a raiz: flexão
        // diferente é palavra diferente aqui, por decisão do usuário.
        tokens.append({ pos, int(bruto.size()), bruto,
                        simplify(lower), idxAtual, paragrafo });
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
        // Todas as ocorrências de uma chave são a mesma palavra, então basta
        // olhar a primeira pra saber se o grupo é de pronome.
        const int janela = isPronoun(tokens[idxs.first()].word.toLower())
                               ? m_pronounProximity : m_proximity;
        const int janelaCruzada = qMin(janela, kCrossParagraphProximity);
        for (int i = 1; i < idxs.size(); ++i) {
            const Token& a = tokens[idxs[i - 1]];
            const Token& b = tokens[idxs[i]];
            const int gap = b.index - a.index;
            if (gap <= (a.paragraph == b.paragraph ? janela : janelaCruzada)) {
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
    // Sem teste de advérbio (inglês, idioma sem dicionário, corretor desligado)
    // esta parte não roda — ver setAdverbTest.
    if (m_adverbTest) {
        QVector<int> adverbios;
        for (int i = 0; i < tokens.size(); ++i) {
            if (m_adverbTest(tokens[i].word.toLower())) adverbios.append(i);
        }
        // Janela dobrada: o vício se mede por parágrafo, não por frase. Dois
        // "-mente" na mesma frase são raros; o que empesta é o punhado deles
        // espalhado pelo mesmo fôlego de leitura. E o fôlego acaba na quebra
        // de parágrafo: advérbio de um parágrafo não empilha com o do próximo.
        const int janela = m_proximity * 2;
        int i = 0;
        while (i < adverbios.size()) {
            int j = i;
            while (j + 1 < adverbios.size()
                   && tokens[adverbios[j + 1]].paragraph == tokens[adverbios[j]].paragraph
                   && tokens[adverbios[j + 1]].index - tokens[adverbios[j]].index <= janela) {
                ++j;
            }
            if (j > i) {   // dois ou mais no mesmo trecho
                Group g;
                g.kind = AdverbPileup;
                g.stem = (m_lang == QLatin1String("fr")) ? QStringLiteral("-ment")
                                                         : QStringLiteral("-mente");
                g.label = g.stem;
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
