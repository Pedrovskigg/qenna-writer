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

    struct Entry {
        QString id;        // GUID
        int blockIndex = 0;
        int start = 0;     // posição absoluta na primeira ocorrência
        int end = 0;
        QString color;     // "#RRGGBB"
        QString comment;
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
    QString applyMarkerToSelection(const QString& docKey,
                                   QTextCursor& cursor,
                                   const QColor& color,
                                   const QString& comment);

    // Remove um marker comentado (por GUID): limpa a property em todos os
    // fragments e tira a cor (volta pra textPrimary do tema).
    void removeMarker(const QString& docKey, QTextDocument* doc, const QString& id);

    // Atualiza cor/comentário de um marker existente (mantém range no doc).
    void updateMarker(const QString& docKey, QTextDocument* doc, const QString& id,
                      const QColor& color, const QString& comment);

    // Lookup direto (usado pelo hover).
    Entry findById(const QString& docKey, const QString& id) const;
    bool hasMarker(const QString& docKey, const QString& id) const;

signals:
    void markersChanged(QString docKey);

private:
    QString sidecarPath() const;

    QString m_root;
    QHash<QString, QVector<Entry>> m_entries;
};
