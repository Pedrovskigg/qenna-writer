#include "ElementsStore.h"

#include "ProjectModel.h"     // só pra ProjectModel::uid()
#include "ProjectStorage.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>

namespace {
QJsonValue stringOrNull(const QString& s) {
    return s.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(s);
}

QString jsonString(const QJsonValue& v) {
    return v.isString() ? v.toString() : QString();
}

QJsonObject elementToJson(const Element& e) {
    QJsonObject o;
    o.insert(QStringLiteral("id"), e.id);
    o.insert(QStringLiteral("name"), e.name);
    o.insert(QStringLiteral("type"), e.type);
    o.insert(QStringLiteral("icon"), e.icon);
    if (!e.image.isEmpty()) o.insert(QStringLiteral("image"), e.image);
    if (!e.role.isEmpty()) o.insert(QStringLiteral("role"), e.role);
    if (!e.trackMode.isEmpty()) o.insert(QStringLiteral("trackMode"), e.trackMode);
    if (e.narrator) o.insert(QStringLiteral("narrator"), true);
    if (!e.textConcordance.isEmpty()) o.insert(QStringLiteral("textConcordance"), e.textConcordance);
    if (!e.aliases.isEmpty()) {
        QJsonArray a;
        for (const auto& al : e.aliases) a.append(al);
        o.insert(QStringLiteral("aliases"), a);
    }
    return o;
}

Element elementFromJson(const QJsonObject& o) {
    Element e;
    e.id = jsonString(o.value(QStringLiteral("id")));
    e.name = jsonString(o.value(QStringLiteral("name")));
    e.type = jsonString(o.value(QStringLiteral("type")));
    e.icon = jsonString(o.value(QStringLiteral("icon")));
    e.image = jsonString(o.value(QStringLiteral("image")));
    e.role = jsonString(o.value(QStringLiteral("role")));
    e.trackMode = jsonString(o.value(QStringLiteral("trackMode")));
    e.narrator = o.value(QStringLiteral("narrator")).toBool(false);
    e.textConcordance = jsonString(o.value(QStringLiteral("textConcordance")));
    const QJsonArray aliases = o.value(QStringLiteral("aliases")).toArray();
    for (const auto& al : aliases) e.aliases.append(jsonString(al));
    return e;
}

QJsonObject typeToJson(const ElementType& t) {
    QJsonObject o;
    o.insert(QStringLiteral("id"), t.id);
    o.insert(QStringLiteral("label"), t.label);
    o.insert(QStringLiteral("color"), t.color);
    o.insert(QStringLiteral("icon"), t.icon);
    return o;
}
}

ElementsStore::ElementsStore(QObject* parent) : QObject(parent) {
    seedDefaultTypes();
}

void ElementsStore::seedDefaultTypes() {
    m_elementTypes.clear();
    m_elementTypes.append({ QStringLiteral("character"), QObject::tr("Personagem"), QStringLiteral("#ffd36a"), QStringLiteral("user") });
    m_elementTypes.append({ QStringLiteral("setting"),   QObject::tr("Cenário"),    QStringLiteral("#7dd3fc"), QStringLiteral("map") });
    m_elementTypes.append({ QStringLiteral("object"),    QObject::tr("Objeto"),     QStringLiteral("#a7f3d0"), QStringLiteral("cube") });
}

void ElementsStore::setProjectRoot(const QString& root) {
    m_root = root;
}

void ElementsStore::clear() {
    m_elements.clear();
    m_elementOrder.clear();
    m_docElements = QJsonObject();
    seedDefaultTypes();
    m_dirty = false;
    emit changed();
}

bool ElementsStore::load() {
    clear();
    if (m_root.isEmpty()) return false;
    const QString path = ProjectStorage::joinPath(m_root, QStringLiteral("elements.json"));
    if (!QFile::exists(path)) return true; // vazio é ok
    bool ok = false;
    const QString text = ProjectStorage::readText(path, &ok);
    if (!ok) return false;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return false;
    loadFromJson(doc.object());
    m_dirty = false;
    emit changed();
    return true;
}

void ElementsStore::loadFromJson(const QJsonObject& root) {
    m_elements.clear();
    m_elementOrder.clear();

    const QJsonArray elementsArr = root.value(QStringLiteral("elements")).toArray();
    for (const auto& v : elementsArr) {
        Element e = elementFromJson(v.toObject());
        if (e.id.isEmpty()) continue;
        m_elements.insert(e.id, e);
        m_elementOrder.append(e.id);
    }

    // Os 3 tipos de elemento (character/setting/object) são fixos e definidos
    // em código — não há UI pra criar tipos customizados. Por isso o array
    // "elementTypes" do JSON (herdado de versões antigas do arquivo) é
    // ignorado aqui: se ele fosse recarregado, o label salvo em disco (preso
    // no idioma de quando o projeto foi criado) sobrescreveria o tr() atual e
    // o combo de "Elemento" nunca acompanharia troca de idioma. m_elementTypes
    // já foi populado por seedDefaultTypes() em clear(), antes desta chamada.

    m_docElements = root.value(QStringLiteral("docElements")).toObject();
}

QJsonObject ElementsStore::toJson() const {
    QJsonObject root;
    QJsonArray elementsArr;
    for (const QString& id : m_elementOrder) {
        auto it = m_elements.constFind(id);
        if (it != m_elements.constEnd()) elementsArr.append(elementToJson(it.value()));
    }
    root.insert(QStringLiteral("elements"), elementsArr);
    QJsonArray typesArr;
    for (const auto& t : m_elementTypes) typesArr.append(typeToJson(t));
    root.insert(QStringLiteral("elementTypes"), typesArr);
    root.insert(QStringLiteral("docElements"), m_docElements);
    root.insert(QStringLiteral("savedAt"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
    root.insert(QStringLiteral("version"), 1);
    return root;
}

bool ElementsStore::save() {
    if (m_root.isEmpty()) return false;
    if (!m_dirty) return true;
    const QString path = ProjectStorage::joinPath(m_root, QStringLiteral("elements.json"));
    const QJsonDocument doc(toJson());
    const QString text = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
    QString err;
    const auto outcome = ProjectStorage::writeTextGuarded(path, text, m_root, QStringLiteral("elements.json"), &err);
    if (outcome == ProjectStorage::WriteOutcome::Written) {
        m_dirty = false;
        return true;
    }
    return false;
}

QList<Element> ElementsStore::elements() const {
    QList<Element> out;
    out.reserve(m_elementOrder.size());
    for (const QString& id : m_elementOrder) {
        auto it = m_elements.constFind(id);
        if (it != m_elements.constEnd()) out.append(it.value());
    }
    return out;
}

const Element* ElementsStore::findElement(const QString& id) const {
    auto it = m_elements.constFind(id);
    return it == m_elements.constEnd() ? nullptr : &it.value();
}

const ElementType* ElementsStore::findType(const QString& id) const {
    for (const auto& t : m_elementTypes) {
        if (t.id == id) return &t;
    }
    return nullptr;
}

QString ElementsStore::addElement(Element e) {
    if (e.id.isEmpty()) e.id = ProjectModel::uid();
    m_elements.insert(e.id, e);
    if (!m_elementOrder.contains(e.id)) m_elementOrder.append(e.id);
    m_dirty = true;
    emit changed();
    return e.id;
}

bool ElementsStore::updateElement(const QString& id, const Element& e) {
    if (!m_elements.contains(id)) return false;
    Element copy = e;
    copy.id = id;
    m_elements.insert(id, copy);
    m_dirty = true;
    emit changed();
    return true;
}

bool ElementsStore::removeElement(const QString& id) {
    if (!m_elements.contains(id)) return false;
    m_elements.remove(id);
    m_elementOrder.removeAll(id);
    m_dirty = true;
    emit changed();
    return true;
}

QStringList ElementsStore::docElementIds(const QString& docKey) const {
    const QJsonArray arr = m_docElements.value(docKey).toArray();
    QStringList result;
    result.reserve(arr.size());
    for (const auto& v : arr) {
        const QString s = v.toString();
        if (!s.isEmpty()) result.append(s);
    }
    return result;
}

bool ElementsStore::hasDocElement(const QString& docKey, const QString& elementId) const {
    const QJsonArray arr = m_docElements.value(docKey).toArray();
    for (const auto& v : arr) {
        if (v.toString() == elementId) return true;
    }
    return false;
}

void ElementsStore::addDocElement(const QString& docKey, const QString& elementId) {
    if (hasDocElement(docKey, elementId)) return;
    QJsonArray arr = m_docElements.value(docKey).toArray();
    arr.append(elementId);
    m_docElements.insert(docKey, arr);
    m_dirty = true;
    emit changed();
}

void ElementsStore::addManyDocElements(const QString& docKey, const QStringList& elementIds) {
    bool anyAdded = false;
    QJsonArray arr = m_docElements.value(docKey).toArray();
    for (const QString& id : elementIds) {
        if (!hasDocElement(docKey, id)) {
            arr.append(id);
            anyAdded = true;
        }
    }
    if (!anyAdded) return;
    m_docElements.insert(docKey, arr);
    m_dirty = true;
    emit changed();
}

void ElementsStore::removeDocElement(const QString& docKey, const QString& elementId) {
    if (!m_docElements.contains(docKey)) return;
    QJsonArray arr = m_docElements.value(docKey).toArray();
    QJsonArray filtered;
    for (const auto& v : arr) {
        if (v.toString() != elementId) filtered.append(v);
    }
    if (filtered.size() == arr.size()) return;
    m_docElements.insert(docKey, filtered);
    m_dirty = true;
    emit changed();
}

QString ElementsStore::elementDocKeyForChapter(const QString& manuscriptId, const QString& chapterId) {
    return DocCache::chapterKey(manuscriptId, chapterId);
}

QString ElementsStore::elementDocKeyForScene(const QString& manuscriptId, const QString& chapterId,
                                              const QString& sceneId) {
    return DocCache::chapterKey(manuscriptId, chapterId) + QStringLiteral("::scene:") + sceneId;
}
