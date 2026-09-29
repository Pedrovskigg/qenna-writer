#pragma once

#include <QChar>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QString>

// Pacote de idioma do detector de diálogos. Pequeno de propósito: o motor lê
// a ESTRUTURA do parágrafo (fala x narração, sujeito da tag, conversa da
// cena) e só precisa de palavras de função — preposições, pronomes, artigos,
// 1ª pessoa. A lista gigante de verbos do motor antigo (DialogueVerbs*.h)
// estourava o limite de tamanho de regex do Qt e deixava a busca por
// proximidade morta em silêncio; aqui não existe nada parecido.
struct DialogueLang {
    QString code; // "pt", "en", "es", "fr", "it"

    // Palavra logo antes do nome que faz dele OBJETO, não sujeito da tag
    // ("olhou para Klara", "disse ao João").
    QSet<QString> preps;
    // Pronome sujeito na tag → gênero ('m'/'f') — "— Entrei. — Ela disse."
    QHash<QString, QChar> pronouns;
    // Possessivo que aponta quem fala ("A voz dela", "Her voice").
    QHash<QString, QChar> possessives;
    bool possessiveLeads = false; // EN: "Her voice" (possessivo vem antes)
    // Artigo no começo da tag = locutor fora do elenco ("O porteiro chamou").
    QSet<QString> articles;
    // "a voz de Klara" — o nome depois de "voz de" É quem fala.
    QSet<QString> voiceNouns;
    // Artigos que votam no gênero do nome seguinte ("a Klara", "o João").
    QSet<QString> femArticles;
    QSet<QString> mascArticles;
    // 1ª pessoa como SUJEITO da tag ("— Não — respondi."). Nunca procurada
    // dentro da fala: "eu" dentro da fala é de quem fala, qualquer um.
    QRegularExpression firstPerson;
    // Incisa dentro da fala, sem travessão de fechamento:
    // "— Je pars, dit Marie." / "— Vamos, disse Ana, e saiu."
    QRegularExpression incise;
    // Verbos de tag mais comuns (os mesmos da incisa): "Respondeu irritada"
    // começa com verbo, não com nome de figurante.
    QSet<QString> tagVerbs;

    // Pacote pelo código ("pt_BR", "en_US"…). Idioma fora dos cinco = pacote
    // neutro: só a parte do motor que não depende de idioma (corte fala/
    // narração, conversa, figurante com maiúscula), sem palavras de outra língua.
    static const DialogueLang& forCode(const QString& code);

    // Pacote pelo próprio texto: conta palavras muito comuns e exclusivas de
    // cada língua ("não", "the", "dijo", "vous", "che"). O idioma do projeto
    // (fallbackCode) só desempata ou vale quando o texto é curto demais —
    // antes, projeto sem corretor marcado num app em inglês lia um livro em
    // português com as regras do inglês.
    static const DialogueLang& detect(const QString& text, const QString& fallbackCode);
};

inline const DialogueLang& DialogueLang::forCode(const QString& rawCode)
{
    const QString code = rawCode.left(2).toLower();
    const auto opts = QRegularExpression::CaseInsensitiveOption
                    | QRegularExpression::UseUnicodePropertiesOption;
    auto words = [](const char* s) {
        QSet<QString> out;
        for (const QString& w : QString::fromUtf8(s).split(QLatin1Char(' '), Qt::SkipEmptyParts))
            out.insert(w);
        return out;
    };
    // Incisa: ", <verbo> Nome" ou ", <verbo>-il". O nome precisa começar com
    // maiúscula (lookahead) pra "Vamos, disse que viria" não virar tag.
    auto verbSet = [](const char* verbs) {
        QSet<QString> out;
        for (const QString& v : QString::fromUtf8(verbs).split(QLatin1Char('|'), Qt::SkipEmptyParts))
            out.insert(v);
        return out;
    };
    auto incise = [&](const char* verbs) {
        return QRegularExpression(
            QStringLiteral(",\\s+(?:%1)(?:-t)?(?:-(?:il|elle|ils|elles|on)\\b|\\s+(?=\\p{Lu}))")
                .arg(QString::fromUtf8(verbs)),
            QRegularExpression::UseUnicodePropertiesOption);
    };

    static const DialogueLang pt = [&] {
        DialogueLang l;
        l.code = QStringLiteral("pt");
        l.preps = words("para pra pro a à ao aos às com de do da dos das em no na nos nas sobre contra "
                        "por pelo pela até entre sem perante");
        l.pronouns = { { QStringLiteral("ele"), QChar('m') }, { QStringLiteral("ela"), QChar('f') } };
        l.possessives = { { QStringLiteral("dele"), QChar('m') }, { QStringLiteral("dela"), QChar('f') } };
        l.articles = words("o a os as um uma");
        l.voiceNouns = words("voz");
        l.femArticles = words("a da na à pela dessa desta");
        l.mascArticles = words("o do no ao pelo desse deste");
        l.firstPerson = QRegularExpression(QStringLiteral(
            "^(?:\\S+\\s+){0,2}(?:eu|falei|respondi|perguntei|gritei|murmurei|sussurrei|retruquei|"
            "comentei|expliquei|pedi|menti|ri|sorri|suspirei|resmunguei|insisti|admiti|repeti|"
            "continuei|completei|emendei|rebati|devolvi|brinquei|confessei|avisei|chamei|"
            "concordei|neguei|garanti|interrompi|arrisquei)(?![\\p{L}\\p{N}])"), opts);
        l.incise = incise("disse|diz|respondeu|perguntou|falou|gritou|murmurou|sussurrou|retrucou|"
                          "continuou|completou|explicou|pediu|exclamou|insistiu|comentou");
        l.tagVerbs = verbSet("disse|diz|respondeu|perguntou|falou|gritou|murmurou|sussurrou|retrucou|"
                          "continuou|completou|explicou|pediu|exclamou|insistiu|comentou");
        return l;
    }();
    static const DialogueLang en = [&] {
        DialogueLang l;
        l.code = QStringLiteral("en");
        l.preps = words("to at with of for from about toward towards into onto on by");
        l.pronouns = { { QStringLiteral("he"), QChar('m') }, { QStringLiteral("she"), QChar('f') } };
        l.possessives = { { QStringLiteral("his"), QChar('m') }, { QStringLiteral("her"), QChar('f') } };
        l.possessiveLeads = true;
        l.articles = words("the a an one");
        l.voiceNouns = words("voice");
        // "I" só maiúsculo — sem CaseInsensitive, senão pegaria "i" solto.
        l.firstPerson = QRegularExpression(QStringLiteral("^(?:\\S+\\s+){0,2}I(?![\\p{L}\\p{N}])"),
                                           QRegularExpression::UseUnicodePropertiesOption);
        l.incise = incise("said|asked|replied|answered|whispered|shouted|muttered|called|cried|added");
        l.tagVerbs = verbSet("said|asked|replied|answered|whispered|shouted|muttered|called|cried|added");
        return l;
    }();
    static const DialogueLang es = [&] {
        DialogueLang l;
        l.code = QStringLiteral("es");
        l.preps = words("a al con de del para hacia sobre contra por en entre sin ante");
        l.pronouns = { { QStringLiteral("él"), QChar('m') }, { QStringLiteral("ella"), QChar('f') } };
        l.articles = words("el la los las un una");
        l.voiceNouns = words("voz");
        l.femArticles = words("la");
        l.mascArticles = words("el");
        l.firstPerson = QRegularExpression(QStringLiteral(
            "^(?:\\S+\\s+){0,2}(?:yo|dije|respondí|pregunté|grité|murmuré|susurré|contesté|repliqué|"
            "expliqué|pedí|reí|sonreí|suspiré|insistí|admití|añadí|mentí)(?![\\p{L}\\p{N}])"), opts);
        l.incise = incise("dijo|preguntó|respondió|contestó|gritó|murmuró|susurró|exclamó|añadió|"
                          "replicó|insistió|explicó|pidió");
        l.tagVerbs = verbSet("dijo|preguntó|respondió|contestó|gritó|murmuró|susurró|exclamó|añadió|"
                          "replicó|insistió|explicó|pidió");
        return l;
    }();
    static const DialogueLang fr = [&] {
        DialogueLang l;
        l.code = QStringLiteral("fr");
        l.preps = words("à au aux avec de du des pour vers sur contre par en chez entre sans");
        l.pronouns = { { QStringLiteral("il"), QChar('m') }, { QStringLiteral("elle"), QChar('f') } };
        l.articles = words("le la les un une l'");
        l.voiceNouns = words("voix");
        l.firstPerson = QRegularExpression(QStringLiteral(
            "(?:^(?:\\S+\\s+){0,2}(?:je|j')|-je(?![\\p{L}\\p{N}]))"), opts);
        // Francês inverte o sujeito SÓ na incisa, então qualquer ", verbo-je"
        // / ", verbo-t-il" é tag, mesmo com verbo fora da lista.
        l.incise = QRegularExpression(
            QStringLiteral(",\\s+(?:(?:dit|demanda|répondit|cria|murmura|chuchota|s'écria|ajouta|reprit|"
                           "répliqua|fit|souffla|lança|expliqua|continua|insista)\\s+(?=\\p{Lu})"
                           "|[\\p{L}']+(?:-t)?-(?:je|il|elle|ils|elles|on)(?![\\p{L}])"
                           // passé simple na 3ª pessoa (soupira, reprit, conclut) + nome ou artigo
                           "|[\\p{L}]+(?:a|it|ut|int)\\s+(?=\\p{Lu}|(?:le|la|les|un|une|l')[\\s\\x{2019}']))"),
            QRegularExpression::UseUnicodePropertiesOption);
        l.tagVerbs = verbSet("dit|demanda|répondit|cria|murmura|chuchota|s'écria|ajouta|reprit|"
                          "répliqua|fit|souffla|lança|expliqua|continua|insista");
        return l;
    }();
    static const DialogueLang it = [&] {
        DialogueLang l;
        l.code = QStringLiteral("it");
        l.preps = words("a al alla ai agli con di del della dei per verso su contro da dal dalla in tra fra senza");
        l.pronouns = { { QStringLiteral("lui"), QChar('m') }, { QStringLiteral("lei"), QChar('f') },
                       { QStringLiteral("egli"), QChar('m') }, { QStringLiteral("ella"), QChar('f') } };
        l.articles = words("il lo la i gli le un una uno l'");
        l.voiceNouns = words("voce");
        l.femArticles = words("la della alla");
        l.mascArticles = words("il del al");
        l.firstPerson = QRegularExpression(QStringLiteral(
            "^(?:\\S+\\s+){0,2}(?:io|dissi|risposi|chiesi|gridai|mormorai|sussurrai|replicai|spiegai|"
            "risi|sorrisi|sospirai|insistetti|ammisi|aggiunsi)(?![\\p{L}\\p{N}])"), opts);
        l.incise = incise("disse|chiese|rispose|gridò|mormorò|sussurrò|esclamò|aggiunse|replicò|"
                          "continuò|spiegò");
        l.tagVerbs = verbSet("disse|chiese|rispose|gridò|mormorò|sussurrò|esclamò|aggiunse|replicò|"
                          "continuò|spiegò");
        return l;
    }();

    static const DialogueLang neutral = [&] {
        DialogueLang l;
        l.code = QStringLiteral("xx");
        l.firstPerson = QRegularExpression(QStringLiteral("(?!)")); // nunca casa
        l.incise = QRegularExpression(QStringLiteral("(?!)"));
        return l;
    }();

    if (code == QLatin1String("pt")) return pt;
    if (code == QLatin1String("en")) return en;
    if (code == QLatin1String("es")) return es;
    if (code == QLatin1String("fr")) return fr;
    if (code == QLatin1String("it")) return it;
    return neutral;
}

inline const DialogueLang& DialogueLang::detect(const QString& text, const QString& fallbackCode)
{
    // Palavras frequentes que quase não aparecem nas outras quatro línguas.
    static const QHash<QString, QString> marker = [] {
        QHash<QString, QString> m;
        const struct { const char* code; const char* words; } langs[] = {
            { "pt", "não você ele ela uma com mas isso também então muito ainda são" },
            { "en", "the and you was said she he what with that his her have" },
            { "es", "el y dijo pero él ella muy usted qué también ahora eso está" },
            { "fr", "le les et je vous pas une dit mais est avec sur elle il" },
            { "it", "che è non disse sono gli della anche perché questo lui lei ha" },
        };
        for (const auto& l : langs)
            for (const QString& w : QString::fromUtf8(l.words).split(QLatin1Char(' ')))
                if (!m.contains(w)) m.insert(w, QString::fromLatin1(l.code));
        return m;
    }();
    static const QRegularExpression wordRe(QStringLiteral("[\\p{L}]+"),
                                           QRegularExpression::UseUnicodePropertiesOption);
    QHash<QString, int> score;
    int total = 0;
    auto it = wordRe.globalMatch(text.left(20000));
    while (it.hasNext()) {
        const auto mw = marker.constFind(it.next().captured(0).toLower());
        if (mw == marker.constEnd()) continue;
        ++score[mw.value()];
        ++total;
    }
    QString best;
    int bestScore = 0;
    for (auto s = score.constBegin(); s != score.constEnd(); ++s)
        if (s.value() > bestScore) { bestScore = s.value(); best = s.key(); }
    const QString fallback = fallbackCode.left(2).toLower();
    // Texto curto demais, ou empate com o idioma do projeto: vale o do projeto.
    if (bestScore < 8 || score.value(fallback) == bestScore) return forCode(fallback);
    return forCode(best);
}
