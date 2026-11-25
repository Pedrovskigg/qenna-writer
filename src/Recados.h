#pragma once
// Recados do autor (P.H. Lobato) pros usuários, lidos do site.
//
// O arquivo mora em https://qennawriter.com/recados.json (docs/recados.json no
// repo), e a mesma lista alimenta a página de blog do site. O app só LÊ esse
// arquivo, igual ao verificador de atualização, e não manda nada de volta. Sem
// internet, fica valendo a última lista baixada (guardada no QSettings), ou
// nada, numa instalação nova.
//
// Formato:
//   { "recados": [ { "id": "2026-11-24-linux", "date": "2026-11-24",
//                    "title": { "pt": "...", "en": "...", ... },
//                    "body":  { "pt": "...", ... },
//                    "link":  { "url": "https://...", "label": { "pt": "...", ... } },  // opcional
//                    "image": "https://qennawriter.com/img/recados/x.webp" } ] }      // opcional
// Idiomas: pt, en, es, it, fr; o que faltar cai no inglês e depois no português.
// QENNA_RECADOS_URL troca o endereço (http ou file://), pra testar um arquivo local.
//
// Comentários (25/11/2026, ideia do Pe): nome + comentário, publicados na hora,
// no app e no blog. Moram num Worker da Cloudflare (tools/comentarios-worker),
// não no site, que só serve arquivo. Travas no servidor: sem link, tamanho
// máximo, poucos envios seguidos; o Pe apaga o que precisar pelo blog.
// QENNA_COMENTARIOS_URL troca o endereço (ex.: http://127.0.0.1:8787/comments).

#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>

class QNetworkAccessManager;

struct Recado {
    QString id;
    QDate date;
    QString title;
    QString body;
    QString linkUrl;
    QString linkLabel;
    QString imageUrl;
};

struct Comentario {
    qint64 id = 0;
    QString name;
    QString body;
    QDateTime at;
};

class RecadosFeed : public QObject {
    Q_OBJECT
public:
    static RecadosFeed* instance();

    // Baixa a lista (uma vez por sessão do app; chamadas repetidas não fazem nada).
    void fetch();
    // Mais novo primeiro, já no idioma do app.
    QList<Recado> recados() const { return m_recados; }

    bool isRead(const QString& id) const { return m_read.contains(id); }
    void markRead(const QString& id);
    void markAllRead();
    // A faixa some quando a pessoa fecha no ×, e só volta com um recado que ela
    // ainda não tinha visto na hora de fechar.
    bool stripDismissed() const;
    void dismissStrip();

    QNetworkAccessManager* network();

    // ── comentários ──
    bool commentsEnabled() const { return !commentsUrl().isEmpty(); }
    void fetchComments();                                   // ao abrir o leitor
    QList<Comentario> comments(const QString& recadoId) const { return m_comments.value(recadoId); }
    bool commentsLoaded() const { return m_commentsLoaded; }
    void postComment(const QString& recadoId, const QString& name, const QString& body);

signals:
    void changed();
    void stateChanged();
    void commentsChanged();
    void commentPosted(const QString& recadoId);
    // reason: no-links, slow-down, empty, too-long, network
    void commentFailed(const QString& recadoId, const QString& reason);

private:
    explicit RecadosFeed(QObject* parent = nullptr);
    void parse(const QByteArray& json);
    void saveState();
    static QString commentsUrl();

    QNetworkAccessManager* m_nam = nullptr;
    QList<Recado> m_recados;
    QSet<QString> m_read;
    QSet<QString> m_dismissedIds;
    bool m_fetched = false;
    QHash<QString, QList<Comentario>> m_comments;
    bool m_commentsLoaded = false;
};
