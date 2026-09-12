#pragma once

#include <QSet>
#include <QString>
#include <QVector>

// Detector de Repetições — acha a mesma palavra (ou palavras da mesma raiz)
// reaparecendo perto demais no texto.
//
// O nome antigo era "Detector de Eco", vetado pelo usuário em 2026-09-12:
// "eco" só diz alguma coisa pra quem já conhece o jargão de oficina literária.
//
// Marca DUAS coisas, e só essas duas:
//
// 1. A MESMA palavra repetida perto. Idêntica, não "da mesma família" —
//    agrupar por raiz foi testado no texto real e reprovado pelo usuário:
//    "ficavam" ecoando "ficar" é parentesco gramatical, não repetição que o
//    leitor ouça. Falso positivo em ferramenta de revisão custa mais caro que
//    achado perdido, porque ensina o autor a ignorar as marcas.
//
// 2. ACÚMULO DE ADVÉRBIOS EM -MENTE, mesmo sendo advérbios diferentes
//    ("rapidamente" ... "lentamente"). É vício clássico de escrita e escapa
//    justamente porque as palavras são diferentes — nenhuma busca por
//    repetição literal pegaria.
class RepetitionDetector {
public:
    // Distância máxima (em PALAVRAS) entre duas ocorrências para contarem como
    // repetição. Longe demais, a repetição deixa de ser audível e vira ruído
    // na lista.
    enum Proximity {
        Tight  = 30,   // mesma frase / frase vizinha
        Normal = 60,   // mesmo parágrafo, aproximadamente
        Loose  = 120,  // parágrafos vizinhos
    };

    struct Occurrence {
        int position = 0;   // offset no texto analisado
        int length = 0;
        QString word;       // a palavra como está escrita
        // Palavras lidas desde a ocorrência anterior DESTE grupo. É o que
        // responde "por que isto está marcado?" — a outra metade do par está
        // atrás, fora da vista, e pode ter outra grafia.
        int wordsSincePrevious = 0;
    };

    enum Kind {
        SameWord,      // a mesma palavra voltou
        AdverbPileup,  // vários advérbios em -mente no mesmo fôlego
    };

    struct Group {
        Kind kind = SameWord;
        QString stem;                  // chave que une as ocorrências
        QString label;                 // a forma mais frequente, para exibir
        QVector<Occurrence> hits;      // em ordem de posição
        int closestGap = 0;            // menor distância em palavras entre duas
    };

    void setProximity(int words) { m_proximity = words; }
    int proximity() const { return m_proximity; }

    // Palavras a ignorar além das funcionais: nomes de personagens e lugares do
    // projeto. Nome próprio repete por necessidade — a alternativa é pronome, e
    // apontar isso como defeito seria só ruído.
    void setIgnoredWords(const QSet<QString>& lowercased) { m_ignored = lowercased; }

    // Idioma do texto ("pt_BR", "en_US", "es_ES") — define a lista de palavras
    // funcionais. Idioma desconhecido cai no português.
    void setLanguage(const QString& langCode);

    // Analisa texto PLANO (não HTML). Os offsets das ocorrências são relativos
    // a ele, então quem chama é responsável por mapear para o documento.
    QVector<Group> analyze(const QString& plainText) const;

    // Palavra curta demais para incomodar quando repete.
    static constexpr int kMinWordLength = 4;

private:
    bool isIgnored(const QString& lowerWord) const;

    int m_proximity = Normal;
    QString m_lang = QStringLiteral("pt");
    QSet<QString> m_ignored;
    QSet<QString> m_stopWords;
};
