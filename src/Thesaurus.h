#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

class QNetworkAccessManager;
class QNetworkReply;

// Dicionário de sinônimos (MyThes, o mesmo formato que o LibreOffice usa).
//
// Os arquivos NÃO são embarcados: somados dão ~36 MB nos três idiomas, e a
// maioria dos autores usa um só. O app baixa sob demanda na primeira consulta e
// guarda em AppData — mesmo caminho já adotado pelo Mira Cover.
//
// Formato do .dat (não tem .idx nestes arquivos, o índice é construído aqui):
//   linha 1: nome da codificação ("UTF-8" no pt_BR)
//   entrada: "palavra|N"  seguida de N linhas de significado
//   significado: "(categoria)primeiro|sinônimo|sinônimo|..."
// Licença dos dados: BSD 3-cláusulas (Kevin B. Hendricks, 2003) no pt_BR;
// inglês e espanhol têm licenças próprias — ver avisos de terceiros.
class Thesaurus : public QObject {
    Q_OBJECT
public:
    // Um sentido da palavra: os sinônimos de uma acepção só. Palavra com várias
    // acepções devolve vários — misturá-los daria sugestões sem sentido.
    struct Sense {
        QString category;      // "Sinônimo", "adj"… no pt_BR é sempre "Sinônimo"
        // Palavra que dá nome à acepção — "casa" no sentido de *armazém*, de
        // *lar*, de *família*. É o que a UI precisa mostrar como cabeçalho:
        // palavra comum chega a 40 acepções, e sem rótulo elas viram uma lista
        // indistinguível.
        QString label;
        QStringList synonyms;
    };

    explicit Thesaurus(QObject* parent = nullptr);
    ~Thesaurus() override;

    // Código no formato que o app já usa pro corretor: "pt_BR", "en_US", "es_ES".
    // Trocar de idioma descarta o índice em memória.
    void setLanguage(const QString& code);
    QString language() const { return m_lang; }

    // Existe arquivo baixado para o idioma atual?
    bool hasData() const;
    // Índice pronto para consulta (implica hasData).
    bool isReady() const { return m_indexed; }
    bool isDownloading() const { return m_reply != nullptr; }

    // Idioma tem thesaurus disponível para download?
    static bool isLanguageSupported(const QString& code);

    // Consulta. Vazio se a palavra não existe, se não há dados ou se o índice
    // ainda não foi montado — quem chama decide se oferece o download.
    QList<Sense> lookup(const QString& word) const;

    // Monta o índice a partir do arquivo já baixado. Custa uma leitura completa
    // (~1s no pt_BR); seguro chamar de novo, não refaz se já estiver pronto.
    bool buildIndex();

    // Baixa o arquivo do idioma atual se ainda não existir.
    void download();
    void cancelDownload();

signals:
    void readyChanged();
    void downloadProgress(qint64 received, qint64 total);
    void downloadFinished(bool ok, QString error);

private:
    QString dataDir() const;
    QString dataPath() const;
    static QString remoteUrlFor(const QString& code);

    QString m_lang;
    bool m_indexed = false;
    bool m_utf8 = true; // codificação declarada na 1ª linha do .dat
    // palavra em minúsculas -> posição, em bytes, da linha "palavra|N".
    // Guardar offset em vez do conteúdo evita segurar 14 MB de texto na RAM.
    QHash<QString, qint64> m_offsets;

    QNetworkAccessManager* m_net = nullptr;
    QNetworkReply* m_reply = nullptr;
};
