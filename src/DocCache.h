#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

class DocCache : public QObject {
    Q_OBJECT
public:
    explicit DocCache(QObject* parent = nullptr);

    static QString chapterKey(const QString& manuscriptId, const QString& chapterId);
    static QString itemKey(const QString& itemId);

    bool has(const QString& key) const;
    QString get(const QString& key); // atualiza LRU

    void set(const QString& key, const QString& html, bool markDirty = true);
    void markClean(const QString& key);
    void markDirty(const QString& key);

    bool isDirty(const QString& key) const;
    QStringList dirtyKeys() const;
    int size() const { return m_content.size(); }

    // Limite de docs na RAM. 0 = sem limite.
    void setMaxDocs(int n);
    int maxDocs() const { return m_maxDocs; }

    // Chaves que nunca serão despejadas (doc aberto no editor + RefMenu).
    void setPinnedKeys(const QSet<QString>& keys);

    void evict(const QString& key);
    void clear();

signals:
    void contentChanged(QString key);
    void dirtyChanged(QString key, bool dirty);
    void evicted(QString key); // doc despejado da RAM (não perdeu dados, só saiu do cache)

private:
    void touchKey(const QString& key);
    void maybeEvict();

    QHash<QString, QString> m_content;
    QSet<QString> m_dirty;
    QList<QString> m_lruOrder; // frente = mais recente
    QSet<QString> m_pinned;
    int m_maxDocs = 0;
};
