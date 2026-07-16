#include "DocCache.h"

DocCache::DocCache(QObject* parent)
    : QObject(parent) {}

QString DocCache::chapterKey(const QString& manuscriptId, const QString& chapterId) {
    const QString ms = manuscriptId.isEmpty() ? QStringLiteral("root") : manuscriptId;
    return QStringLiteral("ch:%1:%2").arg(ms, chapterId);
}

QString DocCache::itemKey(const QString& itemId) {
    return QStringLiteral("it:%1").arg(itemId);
}

bool DocCache::has(const QString& key) const {
    return m_content.contains(key);
}

QString DocCache::get(const QString& key) {
    touchKey(key);
    return m_content.value(key);
}

void DocCache::set(const QString& key, const QString& html, bool markDirtyFlag) {
    const bool existed = m_content.contains(key);
    const bool changed = !existed || m_content.value(key) != html;
    m_content.insert(key, html);
    touchKey(key);
    if (changed) emit contentChanged(key);
    if (markDirtyFlag) markDirty(key);
    if (!existed) maybeEvict();
}

void DocCache::markClean(const QString& key) {
    if (m_dirty.remove(key)) {
        emit dirtyChanged(key, false);
    }
}

void DocCache::markDirty(const QString& key) {
    if (!m_dirty.contains(key)) {
        m_dirty.insert(key);
        emit dirtyChanged(key, true);
    }
}

bool DocCache::isDirty(const QString& key) const {
    return m_dirty.contains(key);
}

QStringList DocCache::dirtyKeys() const {
    return QStringList(m_dirty.cbegin(), m_dirty.cend());
}

void DocCache::setMaxDocs(int n) {
    m_maxDocs = qMax(0, n);
    maybeEvict();
}

void DocCache::setPinnedKeys(const QSet<QString>& keys) {
    m_pinned = keys;
}

void DocCache::evict(const QString& key) {
    // Copia local obrigatória: 'key' pode ser um alias de um elemento de m_lruOrder.
    // removeAll() destrói esse elemento (chama ~QString, libera o d_ptr) e o reference
    // vira dangling. Qualquer leitura posterior de 'key' (inclusive para emitir sinais)
    // leria memória liberada, causando double-free no cleanup do parâmetro do sinal.
    const QString k = key;
    const bool removed = m_content.remove(k) > 0;
    m_dirty.remove(k);
    m_lruOrder.removeAll(k);
    if (removed) {
        emit contentChanged(k);
        emit evicted(k);
    }
}

void DocCache::clear() {
    const QList<QString> keys = m_content.keys();
    const QList<QString> dirties = QList<QString>(m_dirty.cbegin(), m_dirty.cend());
    m_content.clear();
    m_dirty.clear();
    m_lruOrder.clear();
    for (const auto& k : keys) emit contentChanged(k);
    for (const auto& k : dirties) emit dirtyChanged(k, false);
}

void DocCache::touchKey(const QString& key) {
    if (m_maxDocs <= 0) return;
    m_lruOrder.removeAll(key);
    m_lruOrder.prepend(key);
}

void DocCache::maybeEvict() {
    if (m_maxDocs <= 0) return;
    while (m_content.size() > m_maxDocs) {
        bool evicted = false;
        // Percorre do fim (mais antigo) pro início procurando doc evictável.
        for (int i = m_lruOrder.size() - 1; i >= 0; --i) {
            const QString& k = m_lruOrder.at(i);
            if (!m_dirty.contains(k) && !m_pinned.contains(k)) {
                evict(k);
                evicted = true;
                break;
            }
        }
        if (!evicted) break; // todos restantes são dirty ou pinned
    }
}
