#pragma once
// Autocorreção de nomes (ideia do Pe, 2026-10-10). Quando o corretor já marcou
// uma palavra como erro e ela fica perto o bastante de um nome do projeto
// (personagem, cenário, objeto, apelidos), o editor troca pelo nome assim que
// a palavra termina: "Wallidon" vira "Wallison".
//
// A regra de ouro dele: no máximo 1 ou 2 letras de diferença. Em nome curto
// (até 5 letras), só 1 — senão "Maya" vira "Miya" e "Ana" vira "Ada". Letras
// trocadas de lugar contam como uma. Se a palavra ficar igualmente perto de
// dois nomes, não corrige: chute é pior que erro.
//
// Aqui fica só a comparação; quem decide QUANDO perguntar (palavra terminada,
// corretor dizendo que é erro) e como desfazer é o SpellEditor.

#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

class NameAutocorrect {
public:
    // QSettings "editor/autocorrectNames", ligado por padrão.
    static bool enabledSetting();
    static void setEnabledSetting(bool on);

    // Nomes e apelidos inteiros ("Miya Morikawa"): cada palavra vira um alvo.
    // isWord diz se a palavra existe no dicionário — nome que também é palavra
    // comum ("Rosa", "Hope") só corrige quando a pessoa digitou com maiúscula.
    void setNames(const QStringList& names, const std::function<bool(const QString&)>& isWord);
    bool isEmpty() const { return m_names.isEmpty(); }

    // A correção pra uma palavra que o corretor já disse estar errada, ou
    // vazio. Mantém o possessivo ("Wallidon's" -> "Wallison's") e a caixa alta
    // ("WALLIDON" -> "WALLISON").
    QString correctionFor(const QString& word) const;

    // "Wallison's" -> "Wallison": a parte que vira palavra aprendida ao desfazer.
    static QString withoutPossessive(const QString& word);

    // Quantas letras de diferença o nome aceita.
    static int maxDistance(int nameLength) { return nameLength <= 5 ? 1 : 2; }
    // Distância de edição com troca de vizinhas (Damerau, alinhamento ótimo).
    // Para de contar acima de `cap` e devolve cap + 1.
    static int distance(const QString& a, const QString& b, int cap);
    // Minúsculas e sem acento: "Mìya" e "miya" são a mesma coisa pra comparar.
    static QString fold(const QString& s);

private:
    struct Name { QString text; QString folded; bool commonWord = false; };
    QVector<Name> m_names;
};
