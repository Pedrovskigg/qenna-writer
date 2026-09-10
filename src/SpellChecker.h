#pragma once

#include <QList>
#include <QObject>
#include <QPair>
#include <QSet>
#include <QString>
#include <QStringList>

class Hunspell;

class SpellChecker : public QObject {
    Q_OBJECT
public:
    explicit SpellChecker(QObject* parent = nullptr);
    ~SpellChecker() override;

    // Idioma vazio = desligado (nenhuma palavra é marcada como errada).
    void setLanguage(const QString& langCode);
    QString language() const { return m_lang; }
    bool isEnabled() const { return m_hunspell != nullptr; }

    // Raiz do projeto; usada pra localizar/persistir o custom.dic.
    void setProjectRoot(const QString& root);
    QString projectRoot() const { return m_projectRoot; }

    bool isCorrect(const QString& word) const;
    QStringList suggest(const QString& word) const;

    // Forma de dicionário de uma palavra flexionada: "apertou" -> "apertar".
    // Vazio quando o corretor está desligado ou a palavra não é reconhecida.
    // Existe porque o dicionário de sinônimos é indexado por lema, e um romance
    // é feito de verbo conjugado — sem isto, metade das consultas não acha nada.
    QString stemOf(const QString& word) const;

    // Flexiona 'lemma' seguindo o exemplo de 'like': ("pressionar", "apertou")
    // -> "pressionou". Vazio se não conseguir; quem chama cai no lema.
    QString inflectLike(const QString& lemma, const QString& like) const;

    bool addToPersonalDictionary(const QString& word);
    QSet<QString> personalWords() const { return m_personal; }

    // O Glossário (quando vier) alimenta esse set; palavras aqui nunca viram erro.
    void setGlossaryWords(const QSet<QString>& words);

    // Idiomas bundled disponíveis em disco. Retorna {code, label} ordenado.
    static QList<QPair<QString, QString>> availableLanguages();

signals:
    void changed();

private:
    void loadHunspell();
    void unloadHunspell();
    void loadPersonalDictionary();
    void savePersonalDictionary();
    QString personalDictPath() const;
    static QString assetsSpellDir();

    QString m_lang;
    QString m_projectRoot;
    Hunspell* m_hunspell = nullptr;
    QSet<QString> m_personal;
    QSet<QString> m_glossary;
};
