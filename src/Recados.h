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

#include <QDate>
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

signals:
    void changed();
    void stateChanged();

private:
    explicit RecadosFeed(QObject* parent = nullptr);
    void parse(const QByteArray& json);
    void saveState();

    QNetworkAccessManager* m_nam = nullptr;
    QList<Recado> m_recados;
    QSet<QString> m_read;
    QSet<QString> m_dismissedIds;
    bool m_fetched = false;
};
