#pragma once
// "Reportar um bug" (Configurações), desenho do Pe aprovado no Lab de 25/11/2026.
//
// Cada relato passa por 3 estágios: enviado (vermelho), em andamento (amarelo,
// quando o autor responde) e resolvido (verde, com a versão do conserto). Todo
// relato é público (decisão do Pe, 25/11: mostra o serviço e quem chega vê se o
// bug já foi contado); "Meus bugs" acha os da pessoa pelo id anônimo do app, o
// mesmo das reações. No app do autor (com a chave) a seção vira a "Central de bugs".
//
// Mora no mesmo servidor dos comentários (tools/comentarios-worker), rotas /bugs.
// Cada relato tem um protocolo (QW-XXXXX), que nasce junto com o formulário: a
// pessoa pode mandar print ou vídeo por e-mail com ele no assunto.

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>

struct BugReport {
    qint64 id = 0;
    QString code, name, title, body, steps, env;
    QString stage = QStringLiteral("sent");   // sent | work | done
    QString reply, version;
    QDateTime at, replyAt, updated;
};

class BugFeed : public QObject {
    Q_OBJECT
public:
    static BugFeed* instance();

    bool enabled() const;
    bool isAuthor() const;

    // O protocolo do próximo relato: sorteado uma vez e guardado até o envio
    // (fechar e abrir as Configurações não troca o código que a pessoa já copiou).
    QString draftCode();

    void fetchMine();
    void fetchPublic();
    void fetchAll();          // só o autor
    QList<BugReport> mine() const { return m_mine; }
    QList<BugReport> publicBugs() const { return m_public; }
    QList<BugReport> all() const { return m_all; }
    bool mineLoaded() const { return m_mineLoaded; }
    bool publicLoaded() const { return m_publicLoaded; }
    bool allLoaded() const { return m_allLoaded; }

    void submit(const QString& name, const QString& title, const QString& body,
                const QString& steps, const QString& env);
    void update(qint64 id, const QString& stage, const QString& reply, const QString& version);
    void remove(qint64 id);

    // Algum relato da pessoa mudou de estágio ou ganhou resposta desde a última
    // visita à seção (o pontinho no trilho das Configurações).
    bool hasNews() const;
    void markSeen();

signals:
    void changed();
    void submitted(const QString& code);
    void failed(const QString& reason);   // empty, too-long, no-links, slow-down, banned, network
    void adminKeyRejected();

private:
    explicit BugFeed(QObject* parent = nullptr);
    static QString newCode();
    static QString seenKey(const BugReport& b);

    QList<BugReport> m_mine, m_all, m_public;
    bool m_mineLoaded = false, m_publicLoaded = false, m_allLoaded = false;
};
