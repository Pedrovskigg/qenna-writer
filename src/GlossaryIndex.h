#pragma once
// Onde cada termo do glossário aparece no livro: quantas vezes, em quais
// capítulos e a frase da primeira vez. Alimenta o Dicionário (aba do
// Pensário), o popup de adicionar e a ficha do termo no editor.
//
// Lê o texto dos capítulos do DocCache (o que está aberto, com as mudanças
// ainda não salvas) ou do disco, guarda o texto plano de cada capítulo com
// uma assinatura barata (hash do html em memória / data+tamanho do arquivo)
// e só relê o que mudou. Nada roda sozinho em segundo plano: o cálculo
// acontece quando alguém pergunta (aba aberta, popup, hover).

#include <QHash>
#include <QObject>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

class ProjectModel;
class DocCache;
struct Chapter;

class GlossaryIndex : public QObject {
    Q_OBJECT
public:
    struct Usage {
        int count = 0;
        QStringList chapterIds;       // na ordem do livro
        QString firstChapterId;
        QString firstDocKey;          // pro "ir até" (openMarker)
        QString firstSentence;        // a frase inteira, pra mostrar
        QString firstLabel;           // "Capítulo 3"
    };

    GlossaryIndex(ProjectModel* model, DocCache* cache, QObject* parent = nullptr);
    void setProjectRoot(const QString& root);

    Usage usage(const QStringList& spellings) const;
    // O texto mudou (contagem de palavras avisou): as respostas guardadas caem.
    void invalidate();

    // Padrão de busca dos termos: palavra inteira; sigla (tudo maiúsculo)
    // respeita maiúsculas — "ROTA" não casa com "rota" —, o resto ignora.
    static QRegularExpression patternFor(const QStringList& spellings);

private:
    QString plainFor(const Chapter& c) const;

    ProjectModel* m_model = nullptr;
    DocCache* m_cache = nullptr;
    QString m_root;
    struct Plain { QString sig; QString text; };
    mutable QHash<QString, Plain> m_plain;     // chapterId → texto plano
    mutable QHash<QString, Usage> m_usage;     // chave das grafias → resposta
};
