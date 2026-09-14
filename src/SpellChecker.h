#pragma once

#include <QList>
#include <QObject>
#include <QPair>
#include <QSet>
#include <QString>
#include <QStringList>

class Hunspell;
class QNetworkAccessManager;

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

    // Todas as raízes que o dicionário reconhece para a palavra, sem escolher
    // uma. Serve pra perguntas do tipo "esta palavra PODE ser forma de verbo?"
    // — o Detector de Repetições usa isso para não tomar "atormente" (de
    // atormentar) por advérbio em -mente. Vazio com o corretor desligado.
    QStringList stemsOf(const QString& word) const;

    // Flexiona 'lemma' na mesma forma em que 'inflected' está flexionado, dado
    // que 'inflected' é flexão de 'baseLemma':
    //   inflectLike("achar", "chegando", "chegar") -> "achando"
    //
    // O generate() do hunspell não funciona com o dicionário pt_BR (devolve
    // vazio sempre — testado), então a flexão é feita por ANALOGIA: descobre o
    // sufixo que separou a forma do seu lema e aplica o equivalente no alvo,
    // tentando também as outras conjugações. Cada candidato é validado contra o
    // próprio dicionário, então nunca sai palavra inventada.
    QString inflectLike(const QString& lemma, const QString& inflected,
                        const QString& baseLemma) const;

    bool addToPersonalDictionary(const QString& word);
    QSet<QString> personalWords() const { return m_personal; }

    // O Glossário (quando vier) alimenta esse set; palavras aqui nunca viram erro.
    void setGlossaryWords(const QSet<QString>& words);

    // Idiomas instalados (embarcados + baixados). Retorna {code, label} ordenado.
    static QList<QPair<QString, QString>> availableLanguages();

    // ── Dicionário que acompanha o idioma do app ──
    // Decidido em 2026-09-12: quem usa o app em italiano escreve em italiano, e
    // não deveria ter de caçar o dicionário. O projeto guarda "app" em vez de um
    // código fixo, e o código real sai do idioma da interface na hora de usar.
    // Projeto com código explícito (livro em português, app em inglês) não muda.
    static QString followAppValue() { return QStringLiteral("app"); }
    static QString dictionaryForAppLanguage();
    // "app" vira o dicionário do idioma do app; qualquer outro valor passa direto.
    static QString resolveLanguage(const QString& setting);
    static QString labelForLanguage(const QString& code);

    static bool isInstalled(const QString& code);
    // Existe no repositório de dicionários do LibreOffice e sabemos baixar?
    static bool isDownloadable(const QString& code);
    bool isDownloading() const { return !m_downloadingLang.isEmpty(); }
    // Baixa em segundo plano, sem avisos na tela, o dicionário de `code` se ele
    // ainda não existir. Usado na abertura do app com o idioma da interface.
    void ensureDictionary(const QString& code);

signals:
    void changed();
    // Dicionário que não vem com o app começou a ser baixado / terminou.
    void dictionaryDownloadStarted(const QString& code);
    void dictionaryDownloadFinished(const QString& code, bool ok, const QString& error);

private:
    void loadHunspell();
    void unloadHunspell();
    void loadPersonalDictionary();
    void savePersonalDictionary();
    QString personalDictPath() const;
    static QString assetsSpellDir();
    // Onde ficam os dicionários baixados: AppData, nunca a pasta do app.
    static QString downloadedSpellDir();
    // Pasta que contém <code>/index.aff e index.dic, ou vazio.
    static QString dictionaryDir(const QString& code);
    // quiet: não emite os sinais de início/fim (que viram aviso na tela).
    void downloadDictionary(const QString& code, bool quiet = false);

    QNetworkAccessManager* m_net = nullptr;
    QString m_downloadingLang;
    QString m_lang;
    QString m_projectRoot;
    Hunspell* m_hunspell = nullptr;
    QSet<QString> m_personal;
    QSet<QString> m_glossary;
};
