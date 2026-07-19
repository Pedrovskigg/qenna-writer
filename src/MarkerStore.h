#pragma once

#include <QColor>
#include <QHash>
#include <QObject>
#include <QString>
#include <QVector>

class QTextDocument;
class QTextCursor;

// Marker = pintura colorida sobre um trecho do texto, opcionalmente com um
// comentário. Persistência via sidecar JSON `markers.json` no root do projeto,
// indexado pela chave de viewMode do EditorHost. Em runtime, cada marker com
// comentário guarda um GUID numa custom property do QTextCharFormat; markers
// sem comentário vivem apenas como cor (sobrevivem ao HTML round-trip).
class MarkerStore : public QObject {
    Q_OBJECT
public:
    static constexpr int MarkerIdProperty = 0x10A0; // QTextFormat::UserProperty+0xA0

    // Preto ou branco — o que melhor contrasta com `bg` (WCAG luminance).
    // Usado pra escolher cor do texto sobre highlight de marker.
    static QColor pickContrastingFg(const QColor& bg);

    struct Entry {
        QString id;        // GUID
        int blockIndex = 0;
        int start = 0;     // posição absoluta na primeira ocorrência
        int end = 0;
        QString color;     // "#RRGGBB"
        QString comment;
        QString text;      // trecho destacado (snippet), pro agregador do Pensário
        int sceneIndex = -1; // cena (0-based) onde o marker cai; -1 = desconhecida
        qint64 createdAt = 0; // epoch ms da criação; 0 = desconhecido (markers antigos)
    };

    explicit MarkerStore(QObject* parent = nullptr);

    void setProjectRoot(const QString& root);

    // Sidecar IO. load() popula o map, save() escreve.
    bool load();
    bool save() const;

    // Aplica markers do map ao documento (após contentLoaded). Repinta cor e
    // reinjeta a property GUID em char fragments cujos blocos batam com a entry.
    void applyToDocument(const QString& docKey, QTextDocument* doc);

    // Varre o doc e atualiza entries[docKey] preservando metadata (cor/comentário)
    // por GUID. Chamado antes de salvar.
    void captureFromDocument(const QString& docKey, QTextDocument* doc);

    // Aplica novo marker sobre a seleção do cursor. Se comment vazio, não cria
    // entry (só pinta cor). Retorna o GUID criado ("" se sem comentário).
    // sceneIndexHint: cena conhecida pelo chamador (viewMode). Se < 0, o store
    // tenta inferir contando <hr> no documento (válido só quando o doc é o
    // capítulo inteiro, não uma cena isolada).
    QString applyMarkerToSelection(const QString& docKey,
                                   QTextCursor& cursor,
                                   const QColor& color,
                                   const QString& comment,
                                   int sceneIndexHint = -1);

    // Remove um marker comentado (por GUID): limpa a property em todos os
    // fragments e tira a cor (volta pra textPrimary do tema).
    void removeMarker(const QString& docKey, QTextDocument* doc, const QString& id);

    // Limpa qualquer marcador dentro do range do cursor — comentado (delega
    // pra removeMarker, tira a Entry também) ou só cor solta (sem Entry,
    // limpa o QTextCharFormat direto). Cobre o caso de marcador sem
    // comentário, que não tem GUID e por isso não pode ser removido por id.
    void clearRange(const QString& docKey, QTextCursor& cursor);

    // Atualiza cor/comentário de um marker existente (mantém range no doc).
    void updateMarker(const QString& docKey, QTextDocument* doc, const QString& id,
                      const QColor& color, const QString& comment);

    // Lookup direto (usado pelo hover).
    Entry findById(const QString& docKey, const QString& id) const;
    bool hasMarker(const QString& docKey, const QString& id) const;
    bool hasMarkersForKey(const QString& docKey) const;

    // Todos os comentários do projeto, indexados por docKey. Como markers
    // sem comentário não viram Entry, este map já é "todos os comentários".
    // Consumido pelo agregador de Comentários do Pensário.
    const QHash<QString, QVector<Entry>>& allEntries() const { return m_entries; }

signals:
    void markersChanged(QString docKey);

private:
    QString sidecarPath() const;

    QString m_root;
    QHash<QString, QVector<Entry>> m_entries;
};
