#include "MarkerStore.h"
#include "SceneUtils.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPair>
#include <QSaveFile>
#include <QSet>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QUuid>
#include <cmath>

namespace {

QString newGuid()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QColor parseColor(const QString& s)
{
    QColor c(s);
    return c.isValid() ? c : QColor(QStringLiteral("#FFD54F"));
}

}

QColor MarkerStore::pickContrastingFg(const QColor& bg)
{
    auto channel = [](double c) {
        c /= 255.0;
        return (c <= 0.03928) ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
    };
    const double L = 0.2126 * channel(bg.red())
                   + 0.7152 * channel(bg.green())
                   + 0.0722 * channel(bg.blue());
    return (L > 0.179) ? QColor(Qt::black) : QColor(Qt::white);
}

MarkerStore::MarkerStore(QObject* parent)
    : QObject(parent)
{
}

void MarkerStore::setProjectRoot(const QString& root)
{
    if (m_root == root) return;
    m_root = root;
    m_entries.clear();
}

QString MarkerStore::sidecarPath() const
{
    if (m_root.isEmpty()) return QString();
    return QDir::cleanPath(m_root + QStringLiteral("/markers.json"));
}

bool MarkerStore::load()
{
    m_entries.clear();
    const QString path = sidecarPath();
    if (path.isEmpty()) return false;
    QFile f(path);
    if (!f.exists()) return true; // sem markers ainda, ok
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray data = f.readAll();
    f.close();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return false;
    const QJsonObject root = doc.object();
    for (auto it = root.constBegin(); it != root.constEnd(); ++it) {
        const QString key = it.key();
        if (!it.value().isArray()) continue;
        QVector<Entry> list;
        for (const auto& v : it.value().toArray()) {
            const QJsonObject o = v.toObject();
            Entry e;
            e.id = o.value(QStringLiteral("id")).toString();
            e.blockIndex = o.value(QStringLiteral("blockIndex")).toInt();
            e.start = o.value(QStringLiteral("start")).toInt();
            e.end = o.value(QStringLiteral("end")).toInt();
            e.color = o.value(QStringLiteral("color")).toString();
            e.comment = o.value(QStringLiteral("comment")).toString();
            e.text = o.value(QStringLiteral("text")).toString();
            e.sceneIndex = o.value(QStringLiteral("sceneIndex")).toInt(-1);
            e.createdAt = o.value(QStringLiteral("createdAt")).toVariant().toLongLong();
            if (e.id.isEmpty()) continue;
            list.append(e);
        }
        if (!list.isEmpty()) m_entries.insert(key, list);
    }
    return true;
}

bool MarkerStore::save() const
{
    const QString path = sidecarPath();
    if (path.isEmpty()) return false;

    QJsonObject root;
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        QJsonArray arr;
        for (const Entry& e : it.value()) {
            QJsonObject o;
            o.insert(QStringLiteral("id"), e.id);
            o.insert(QStringLiteral("blockIndex"), e.blockIndex);
            o.insert(QStringLiteral("start"), e.start);
            o.insert(QStringLiteral("end"), e.end);
            o.insert(QStringLiteral("color"), e.color);
            o.insert(QStringLiteral("comment"), e.comment);
            if (!e.text.isEmpty()) o.insert(QStringLiteral("text"), e.text);
            if (e.sceneIndex >= 0) o.insert(QStringLiteral("sceneIndex"), e.sceneIndex);
            if (e.createdAt > 0) o.insert(QStringLiteral("createdAt"), e.createdAt);
            arr.append(o);
        }
        if (!arr.isEmpty()) root.insert(it.key(), arr);
    }

    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return f.commit();
}

void MarkerStore::applyToDocument(const QString& docKey, QTextDocument* doc)
{
    if (!doc) return;
    const auto it = m_entries.constFind(docKey);
    if (it == m_entries.constEnd()) return;

    for (const Entry& e : it.value()) {
        QTextBlock block = doc->findBlockByNumber(e.blockIndex);
        if (!block.isValid()) continue;
        const int blockStart = block.position();
        const int blockLen = block.length() - 1;
        const int from = blockStart + qBound(0, e.start - blockStart, blockLen);
        const int to = blockStart + qBound(0, e.end - blockStart, blockLen);
        if (to <= from) continue;

        QTextCursor c(doc);
        c.setPosition(from);
        c.setPosition(to, QTextCursor::KeepAnchor);
        QTextCharFormat fmt;
        const QColor bg = parseColor(e.color);
        fmt.setBackground(bg);
        fmt.setForeground(pickContrastingFg(bg));
        fmt.setProperty(MarkerIdProperty, e.id);
        c.mergeCharFormat(fmt);
    }
}

void MarkerStore::captureFromDocument(const QString& docKey, QTextDocument* doc)
{
    if (!doc) return;

    QVector<Entry> oldList = m_entries.value(docKey);
    QHash<QString, Entry> oldById;
    for (const Entry& e : oldList) oldById.insert(e.id, e);

    QVector<Entry> fresh;
    QHash<QString, int> seenInFresh; // id -> idx em fresh (pra unir contíguos)

    for (QTextBlock block = doc->firstBlock(); block.isValid(); block = block.next()) {
        const int blockIdx = block.blockNumber();
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment frag = it.fragment();
            if (!frag.isValid()) continue;
            const QTextCharFormat fmt = frag.charFormat();
            if (!fmt.hasProperty(MarkerIdProperty)) continue;
            const QString id = fmt.property(MarkerIdProperty).toString();
            if (id.isEmpty()) continue;

            const int fStart = frag.position();
            const int fEnd = fStart + frag.length();

            if (seenInFresh.contains(id)) {
                Entry& last = fresh[seenInFresh.value(id)];
                if (last.blockIndex == blockIdx && last.end == fStart) {
                    last.end = fEnd;
                    continue;
                }
            }

            Entry e;
            e.id = id;
            e.blockIndex = blockIdx;
            e.start = fStart;
            e.end = fEnd;
            const Entry& prev = oldById.value(id);
            e.color = prev.color.isEmpty()
                ? fmt.background().color().name()
                : prev.color;
            e.comment = prev.comment;
            e.text = prev.text;
            // Preserva: recalcular via <hr> erraria quando o doc é uma cena
            // isolada (sem separadores). O valor vem certo da criação.
            e.sceneIndex = prev.sceneIndex;
            e.createdAt = prev.createdAt;
            seenInFresh.insert(id, fresh.size());
            fresh.append(e);
        }
    }

    if (fresh.isEmpty()) {
        m_entries.remove(docKey);
    } else {
        m_entries.insert(docKey, fresh);
    }
}

QString MarkerStore::applyMarkerToSelection(const QString& docKey,
                                            QTextCursor& cursor,
                                            const QColor& color,
                                            const QString& comment,
                                            int sceneIndexHint)
{
    if (!cursor.hasSelection()) return QString();
    QTextCharFormat fmt;
    fmt.setBackground(color);
    fmt.setForeground(pickContrastingFg(color));

    // Trecho destacado (normaliza separador de parágrafo do Qt) — guardado pro
    // agregador do Pensário exibir/navegar até o trecho.
    QString snippet = cursor.selectedText();
    snippet.replace(QChar(0x2029), QChar(' '));

    QString id;
    if (!comment.trimmed().isEmpty()) {
        id = newGuid();
        fmt.setProperty(MarkerIdProperty, id);
    }
    cursor.mergeCharFormat(fmt);

    if (!id.isEmpty()) {
        Entry e;
        e.id = id;
        const int selStart = cursor.selectionStart();
        const int selEnd = cursor.selectionEnd();
        QTextCursor probe(cursor.document());
        probe.setPosition(selStart);
        e.blockIndex = probe.blockNumber();
        e.start = selStart;
        e.end = selEnd;
        e.color = color.name();
        e.comment = comment;
        e.text = snippet;
        e.sceneIndex = (sceneIndexHint >= 0)
            ? sceneIndexHint
            : SceneUtils::sceneIndexForBlock(cursor.document(), e.blockIndex);
        e.createdAt = QDateTime::currentMSecsSinceEpoch();
        m_entries[docKey].append(e);
        emit markersChanged(docKey);
    }
    return id;
}

void MarkerStore::removeMarker(const QString& docKey, QTextDocument* doc, const QString& id)
{
    if (id.isEmpty() || !doc) return;

    for (QTextBlock block = doc->firstBlock(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment frag = it.fragment();
            if (!frag.isValid()) continue;
            const QTextCharFormat fmt = frag.charFormat();
            if (fmt.property(MarkerIdProperty).toString() != id) continue;

            QTextCursor c(doc);
            c.setPosition(frag.position());
            c.setPosition(frag.position() + frag.length(), QTextCursor::KeepAnchor);
            QTextCharFormat clear = c.charFormat();
            clear.clearProperty(MarkerIdProperty);
            clear.clearBackground();
            clear.clearForeground();
            c.setCharFormat(clear);
        }
    }

    auto& list = m_entries[docKey];
    list.erase(std::remove_if(list.begin(), list.end(),
                              [&](const Entry& e) { return e.id == id; }),
               list.end());
    if (list.isEmpty()) m_entries.remove(docKey);
    emit markersChanged(docKey);
}

void MarkerStore::clearRange(const QString& docKey, QTextCursor& cursor)
{
    if (!cursor.hasSelection()) return;
    QTextDocument* doc = cursor.document();
    if (!doc) return;
    const int selStart = cursor.selectionStart();
    const int selEnd = cursor.selectionEnd();

    // Primeira passada: separa fragmentos com GUID (marcador comentado — vai
    // pro removeMarker, que também limpa a Entry) dos fragmentos que só têm
    // cor solta (marcador sem comentário — não tem id, precisa limpar aqui).
    QSet<QString> idsToRemove;
    QVector<QPair<int, int>> plainRanges;
    for (QTextBlock block = doc->findBlock(selStart);
         block.isValid() && block.position() < selEnd; block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment frag = it.fragment();
            if (!frag.isValid()) continue;
            const int fStart = frag.position();
            const int fEnd = fStart + frag.length();
            if (fEnd <= selStart || fStart >= selEnd) continue;
            const QTextCharFormat fmt = frag.charFormat();
            const QString id = fmt.property(MarkerIdProperty).toString();
            if (!id.isEmpty()) { idsToRemove.insert(id); continue; }
            if (fmt.background().style() != Qt::NoBrush)
                plainRanges.append({ qMax(fStart, selStart), qMin(fEnd, selEnd) });
        }
    }

    for (const QString& id : std::as_const(idsToRemove))
        removeMarker(docKey, doc, id);

    // Cada range limpo isoladamente (não a seleção inteira de uma vez) pra
    // preservar negrito/itálico/etc. de cada fragmento — um setCharFormat
    // sobre a seleção toda usaria só o format do primeiro caractere.
    for (const auto& range : plainRanges) {
        QTextCursor c(doc);
        c.setPosition(range.first);
        c.setPosition(range.second, QTextCursor::KeepAnchor);
        QTextCharFormat clear = c.charFormat();
        clear.clearBackground();
        clear.clearForeground();
        c.setCharFormat(clear);
    }
}

void MarkerStore::updateMarker(const QString& docKey, QTextDocument* doc, const QString& id,
                               const QColor& color, const QString& comment)
{
    if (id.isEmpty() || !doc) return;

    for (QTextBlock block = doc->firstBlock(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment frag = it.fragment();
            if (!frag.isValid()) continue;
            const QTextCharFormat fmt = frag.charFormat();
            if (fmt.property(MarkerIdProperty).toString() != id) continue;

            QTextCursor c(doc);
            c.setPosition(frag.position());
            c.setPosition(frag.position() + frag.length(), QTextCursor::KeepAnchor);
            QTextCharFormat next;
            next.setBackground(color);
            next.setForeground(pickContrastingFg(color));
            next.setProperty(MarkerIdProperty, id);
            c.mergeCharFormat(next);
        }
    }

    auto& list = m_entries[docKey];
    for (Entry& e : list) {
        if (e.id == id) {
            e.color = color.name();
            e.comment = comment;
        }
    }
    emit markersChanged(docKey);
}

MarkerStore::Entry MarkerStore::findById(const QString& docKey, const QString& id) const
{
    const auto it = m_entries.constFind(docKey);
    if (it == m_entries.constEnd()) return Entry{};
    for (const Entry& e : it.value()) {
        if (e.id == id) return e;
    }
    return Entry{};
}

bool MarkerStore::hasMarker(const QString& docKey, const QString& id) const
{
    return !findById(docKey, id).id.isEmpty();
}

bool MarkerStore::hasMarkersForKey(const QString& docKey) const
{
    auto it = m_entries.constFind(docKey);
    return it != m_entries.constEnd() && !it->isEmpty();
}
