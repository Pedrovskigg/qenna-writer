#include "ProjectModel.h"
#include "SceneUtils.h"

#include <QJsonValue>
#include <QJsonDocument>
#include <QRandomGenerator>

namespace {

QString jsonString(const QJsonValue& v) {
    return v.isString() ? v.toString() : QString();
}

QString jsonStringOrEmpty(const QJsonValue& v) {
    if (v.isString()) return v.toString();
    return QString();
}

QJsonValue stringOrNull(const QString& s) {
    return s.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(s);
}

QJsonObject variationToJson(const Variation& v) {
    QJsonObject o;
    o.insert(QStringLiteral("id"), v.id);
    o.insert(QStringLiteral("label"), v.label);
    o.insert(QStringLiteral("isPrimary"), v.isPrimary);
    return o;
}

Variation variationFromJson(const QJsonObject& o) {
    Variation v;
    v.id = jsonString(o.value(QStringLiteral("id")));
    v.label = jsonString(o.value(QStringLiteral("label")));
    v.isPrimary = o.value(QStringLiteral("isPrimary")).toBool(false);
    return v;
}

QJsonObject sceneToJson(const Scene& s) {
    QJsonObject o;
    o.insert(QStringLiteral("id"), s.id);
    o.insert(QStringLiteral("title"), s.title);
    o.insert(QStringLiteral("order"), s.order);
    if (!s.variations.isEmpty()) {
        QJsonArray arr;
        for (const auto& v : s.variations) arr.append(variationToJson(v));
        o.insert(QStringLiteral("variations"), arr);
    }
    if (!s.activeVariationId.isEmpty()) {
        o.insert(QStringLiteral("activeVariationId"), s.activeVariationId);
    }
    return o;
}

Scene sceneFromJson(const QJsonObject& o) {
    Scene s;
    s.id = jsonString(o.value(QStringLiteral("id")));
    s.title = jsonString(o.value(QStringLiteral("title")));
    s.order = o.value(QStringLiteral("order")).toInt(0);
    const QJsonArray vars = o.value(QStringLiteral("variations")).toArray();
    for (const auto& vv : vars) s.variations.append(variationFromJson(vv.toObject()));
    s.activeVariationId = jsonString(o.value(QStringLiteral("activeVariationId")));
    return s;
}

QJsonObject chapterToJson(const Chapter& c, int fallbackOrder) {
    QJsonObject o;
    o.insert(QStringLiteral("id"), c.id);
    o.insert(QStringLiteral("manuscriptId"), stringOrNull(c.manuscriptId));
    o.insert(QStringLiteral("title"), c.title);
    o.insert(QStringLiteral("file"), c.file);
    o.insert(QStringLiteral("order"), c.order > 0 ? c.order : fallbackOrder);
    if (c.scenes.size() > 1) {
        QJsonArray arr;
        for (const auto& s : c.scenes) arr.append(sceneToJson(s));
        o.insert(QStringLiteral("scenes"), arr);
    }
    return o;
}

Chapter chapterFromJson(const QJsonObject& o) {
    Chapter c;
    c.id = jsonString(o.value(QStringLiteral("id")));
    c.manuscriptId = jsonStringOrEmpty(o.value(QStringLiteral("manuscriptId")));
    c.title = jsonString(o.value(QStringLiteral("title")));
    c.file = jsonString(o.value(QStringLiteral("file")));
    c.order = o.value(QStringLiteral("order")).toInt(0);
    const QJsonArray scenes = o.value(QStringLiteral("scenes")).toArray();
    for (const auto& sv : scenes) c.scenes.append(sceneFromJson(sv.toObject()));
    return c;
}

QJsonObject manuscriptToJson(const Manuscript& m) {
    QJsonObject o;
    o.insert(QStringLiteral("id"), m.id);
    o.insert(QStringLiteral("title"), m.title);
    o.insert(QStringLiteral("html"), m.html);
    return o;
}

Manuscript manuscriptFromJson(const QJsonObject& o) {
    Manuscript m;
    m.id = jsonString(o.value(QStringLiteral("id")));
    m.title = jsonString(o.value(QStringLiteral("title")));
    m.html = jsonString(o.value(QStringLiteral("html")));
    return m;
}

QJsonObject folderToJson(const Folder& f) {
    QJsonObject o;
    o.insert(QStringLiteral("id"), f.id);
    o.insert(QStringLiteral("title"), f.title);
    o.insert(QStringLiteral("parentId"), stringOrNull(f.parentId));
    o.insert(QStringLiteral("markerId"), stringOrNull(f.markerId));
    return o;
}

Folder folderFromJson(const QJsonObject& o) {
    Folder f;
    f.id = jsonString(o.value(QStringLiteral("id")));
    f.title = jsonString(o.value(QStringLiteral("title")));
    f.parentId = jsonStringOrEmpty(o.value(QStringLiteral("parentId")));
    f.markerId = jsonStringOrEmpty(o.value(QStringLiteral("markerId")));
    return f;
}

QJsonObject drawerItemToJson(const DrawerItem& it) {
    QJsonObject o;
    o.insert(QStringLiteral("id"), it.id);
    o.insert(QStringLiteral("title"), it.title);
    o.insert(QStringLiteral("folderId"), stringOrNull(it.folderId));
    o.insert(QStringLiteral("markerId"), stringOrNull(it.markerId));
    o.insert(QStringLiteral("elementId"), stringOrNull(it.elementId));
    o.insert(QStringLiteral("elementType"), stringOrNull(it.elementType));
    o.insert(QStringLiteral("elementIcon"), stringOrNull(it.elementIcon));
    if (!it.file.isEmpty()) {
        o.insert(QStringLiteral("file"), it.file);
    } else {
        o.insert(QStringLiteral("html"), it.hasInlineHtml ? it.html : QStringLiteral("<p></p>"));
    }
    o.insert(QStringLiteral("isChapter"), it.isChapter);
    o.insert(QStringLiteral("chapterRef"), stringOrNull(it.chapterRef));
    o.insert(QStringLiteral("role"), stringOrNull(it.role));
    if (!it.charStatus.isEmpty())       o.insert(QStringLiteral("charStatus"), it.charStatus);
    if (!it.charStatusDetail.isEmpty()) o.insert(QStringLiteral("charStatusDetail"), it.charStatusDetail);
    if (!it.charLocation.isEmpty())     o.insert(QStringLiteral("charLocation"), it.charLocation);
    return o;
}

DrawerItem drawerItemFromJson(const QJsonObject& o) {
    DrawerItem it;
    it.id = jsonString(o.value(QStringLiteral("id")));
    it.title = jsonString(o.value(QStringLiteral("title")));
    it.folderId = jsonStringOrEmpty(o.value(QStringLiteral("folderId")));
    it.markerId = jsonStringOrEmpty(o.value(QStringLiteral("markerId")));
    it.elementId = jsonStringOrEmpty(o.value(QStringLiteral("elementId")));
    it.elementType = jsonStringOrEmpty(o.value(QStringLiteral("elementType")));
    it.elementIcon = jsonStringOrEmpty(o.value(QStringLiteral("elementIcon")));
    it.file = jsonStringOrEmpty(o.value(QStringLiteral("file")));
    if (o.contains(QStringLiteral("html"))) {
        it.html = jsonStringOrEmpty(o.value(QStringLiteral("html")));
        it.hasInlineHtml = true;
    }
    it.isChapter = o.value(QStringLiteral("isChapter")).toBool(false);
    it.chapterRef = jsonStringOrEmpty(o.value(QStringLiteral("chapterRef")));
    it.role = jsonStringOrEmpty(o.value(QStringLiteral("role")));
    it.charStatus       = jsonStringOrEmpty(o.value(QStringLiteral("charStatus")));
    it.charStatusDetail = jsonStringOrEmpty(o.value(QStringLiteral("charStatusDetail")));
    it.charLocation     = jsonStringOrEmpty(o.value(QStringLiteral("charLocation")));
    return it;
}

QJsonObject drawerToJson(const Drawer& d) {
    QJsonObject o;
    o.insert(QStringLiteral("key"), d.key);
    o.insert(QStringLiteral("title"), d.title);
    o.insert(QStringLiteral("color"), d.color);
    o.insert(QStringLiteral("drawerIcon"), stringOrNull(d.drawerIcon));
    o.insert(QStringLiteral("drawerElementType"), stringOrNull(d.drawerElementType));
    o.insert(QStringLiteral("drawerElementIcon"), stringOrNull(d.drawerElementIcon));
    QJsonArray folders;
    for (const auto& f : d.folders) folders.append(folderToJson(f));
    o.insert(QStringLiteral("folders"), folders);
    QJsonArray items;
    for (const auto& it : d.items) items.append(drawerItemToJson(it));
    o.insert(QStringLiteral("items"), items);
    return o;
}

QJsonObject characterBondToJson(const CharacterBond& b) {
    QJsonObject o;
    o.insert(QStringLiteral("id"), b.id);
    o.insert(QStringLiteral("drawerKey"), b.drawerKey);
    o.insert(QStringLiteral("fromItemId"), b.fromItemId);
    o.insert(QStringLiteral("toItemId"), b.toItemId);
    o.insert(QStringLiteral("type"), b.type);
    o.insert(QStringLiteral("description"), b.description);
    o.insert(QStringLiteral("color"), b.color);
    return o;
}

CharacterBond characterBondFromJson(const QJsonObject& o) {
    CharacterBond b;
    b.id = jsonString(o.value(QStringLiteral("id")));
    b.drawerKey = jsonString(o.value(QStringLiteral("drawerKey")));
    b.fromItemId = jsonString(o.value(QStringLiteral("fromItemId")));
    b.toItemId = jsonString(o.value(QStringLiteral("toItemId")));
    b.type = jsonString(o.value(QStringLiteral("type")));
    b.description = jsonString(o.value(QStringLiteral("description")));
    b.color = jsonString(o.value(QStringLiteral("color")));
    return b;
}

Drawer drawerFromJson(const QJsonObject& o) {
    Drawer d;
    d.key = jsonString(o.value(QStringLiteral("key")));
    d.title = jsonString(o.value(QStringLiteral("title")));
    d.color = jsonString(o.value(QStringLiteral("color")));
    d.drawerIcon = jsonStringOrEmpty(o.value(QStringLiteral("drawerIcon")));
    d.drawerElementType = jsonStringOrEmpty(o.value(QStringLiteral("drawerElementType")));
    d.drawerElementIcon = jsonStringOrEmpty(o.value(QStringLiteral("drawerElementIcon")));
    const QJsonArray folders = o.value(QStringLiteral("folders")).toArray();
    for (const auto& fv : folders) d.folders.append(folderFromJson(fv.toObject()));
    const QJsonArray items = o.value(QStringLiteral("items")).toArray();
    for (const auto& iv : items) d.items.append(drawerItemFromJson(iv.toObject()));
    return d;
}

} // namespace

ProjectModel::ProjectModel(QObject* parent)
    : QObject(parent) {}

void ProjectModel::setProjectName(const QString& name) {
    if (m_projectName == name) return;
    m_projectName = name;
    emit projectNameChanged();
}

void ProjectModel::setManuscripts(const QList<Manuscript>& list) {
    m_manuscripts = list;
    emit manuscriptsChanged();
}

void ProjectModel::setChapters(const QList<Chapter>& list) {
    m_chapters = list;
    emit chaptersChanged();
}

void ProjectModel::setDrawers(const QList<Drawer>& list) {
    m_drawers = list;
    emit drawersChanged();
}

void ProjectModel::setActiveManuscriptId(const QString& id) {
    if (m_activeManuscriptId == id) return;
    m_activeManuscriptId = id;
    emit activeManuscriptChanged();
}

void ProjectModel::setActiveChapterId(const QString& id) {
    if (m_activeChapterId == id) return;
    m_activeChapterId = id;
    emit activeChapterChanged();
}

void ProjectModel::setSettings(const QJsonObject& s) {
    m_settings = s;
    emit settingsChanged();
}

bool ProjectModel::firstLineIndentEnabled() const {
    const auto v = m_settings.value(QStringLiteral("firstLineIndent"));
    if (v.isUndefined() || v.isNull()) return true;
    return v.toBool(true);
}

void ProjectModel::setFirstLineIndentEnabled(bool enabled) {
    if (firstLineIndentEnabled() == enabled) return;
    m_settings.insert(QStringLiteral("firstLineIndent"), enabled);
    emit settingsChanged();
}

int ProjectModel::paragraphSpacingBefore() const {
    const auto v = m_settings.value(QStringLiteral("paragraphSpacingBefore"));
    if (v.isUndefined() || v.isNull()) return 0;
    return qBound(0, v.toInt(0), 32);
}

void ProjectModel::setParagraphSpacingBefore(int px) {
    const int clamped = qBound(0, px, 32);
    if (paragraphSpacingBefore() == clamped) return;
    m_settings.insert(QStringLiteral("paragraphSpacingBefore"), clamped);
    emit settingsChanged();
}

int ProjectModel::paragraphSpacingAfter() const {
    const auto v = m_settings.value(QStringLiteral("paragraphSpacingAfter"));
    if (v.isUndefined() || v.isNull()) return 0;
    return qBound(0, v.toInt(0), 32);
}

void ProjectModel::setParagraphSpacingAfter(int px) {
    const int clamped = qBound(0, px, 32);
    if (paragraphSpacingAfter() == clamped) return;
    m_settings.insert(QStringLiteral("paragraphSpacingAfter"), clamped);
    emit settingsChanged();
}

int ProjectModel::lineHeightPercent() const {
    const auto v = m_settings.value(QStringLiteral("lineHeightPercent"));
    if (v.isUndefined() || v.isNull()) return 170;
    return qBound(80, v.toInt(170), 400);
}

void ProjectModel::setLineHeightPercent(int percent) {
    const int clamped = qBound(80, percent, 400);
    if (lineHeightPercent() == clamped) return;
    m_settings.insert(QStringLiteral("lineHeightPercent"), clamped);
    emit settingsChanged();
}

QString ProjectModel::spellLanguage() const {
    const auto v = m_settings.value(QStringLiteral("spellLanguage"));
    if (v.isUndefined() || v.isNull()) return QString();
    return v.toString();
}

void ProjectModel::setSpellLanguage(const QString& code) {
    if (spellLanguage() == code) return;
    if (code.isEmpty()) {
        m_settings.remove(QStringLiteral("spellLanguage"));
    } else {
        m_settings.insert(QStringLiteral("spellLanguage"), code);
    }
    emit settingsChanged();
}

void ProjectModel::setUi(const QJsonObject& u) {
    m_ui = u;
    emit uiChanged();
}

void ProjectModel::setDataExtras(const QJsonObject& d) {
    m_dataExtras = d;
}

QString ProjectModel::projectAuthor() const {
    return m_dataExtras.value(QStringLiteral("projectDetails")).toObject()
        .value(QStringLiteral("author")).toString();
}

QString ProjectModel::projectGenres() const {
    return m_dataExtras.value(QStringLiteral("projectDetails")).toObject()
        .value(QStringLiteral("genres")).toString();
}

QString ProjectModel::projectSynopsis() const {
    return m_dataExtras.value(QStringLiteral("projectDetails")).toObject()
        .value(QStringLiteral("synopsis")).toString();
}

QString ProjectModel::projectCoverDataUrl() const {
    const QJsonObject pd = m_dataExtras.value(QStringLiteral("projectDetails")).toObject();
    const QString full = pd.value(QStringLiteral("coverFull")).toString();
    if (!full.isEmpty()) return full;
    return pd.value(QStringLiteral("cover")).toString();
}

void ProjectModel::setProjectDetails(const QString& name, const QString& author,
                                     const QString& genres, const QString& synopsis,
                                     const QString& coverDataUrl) {
    bool changed = false;

    if (m_projectName != name) {
        m_projectName = name;
        emit projectNameChanged();
        changed = true;
    }

    QJsonObject pd = m_dataExtras.value(QStringLiteral("projectDetails")).toObject();
    auto setStr = [&](const QString& key, const QString& value) {
        const QString cur = pd.value(key).toString();
        if (cur == value) return;
        if (value.isEmpty()) pd.remove(key);
        else pd.insert(key, value);
        changed = true;
    };
    setStr(QStringLiteral("author"), author);
    setStr(QStringLiteral("genres"), genres);
    setStr(QStringLiteral("synopsis"), synopsis);
    // Compat Mira 1: gravamos em ambas as chaves (cover e coverFull).
    setStr(QStringLiteral("cover"), coverDataUrl);
    setStr(QStringLiteral("coverFull"), coverDataUrl);

    if (!pd.isEmpty()) m_dataExtras.insert(QStringLiteral("projectDetails"), pd);
    else m_dataExtras.remove(QStringLiteral("projectDetails"));

    if (changed) emit projectDetailsChanged();
}

void ProjectModel::addDrawer(const Drawer& drawer) {
    m_drawers.append(drawer);
    emit drawersChanged();
}

void ProjectModel::seedFromTemplate(const QString& templateId) {
    auto mkItem = [](const QString& title) {
        DrawerItem it;
        it.id = uid();
        it.title = title;
        it.html = QStringLiteral("<p></p>");
        it.hasInlineHtml = true;
        return it;
    };
    auto mkDrawer = [](const QString& title, const QString& color,
                       const QString& elementType = QString(),
                       const QString& elementIcon = QString(),
                       const QList<DrawerItem>& items = {}) {
        Drawer d;
        d.key = QStringLiteral("drawer_") + uid();
        d.title = title;
        d.color = color;
        d.drawerElementType = elementType;
        d.drawerElementIcon = elementIcon;
        d.items = items;
        return d;
    };

    QList<Drawer> seeds;
    if (templateId == QLatin1String("basic")) {
        seeds << mkDrawer(QStringLiteral("Planejamento"), QStringLiteral("#2b79ff"),
                          QString(), QString(),
                          { mkItem(QStringLiteral("Sinopse")),
                            mkItem(QStringLiteral("Fios narrativos")),
                            mkItem(QStringLiteral("Ambientação")) });
        seeds << mkDrawer(QStringLiteral("Personagens"), QStringLiteral("#ff4d4d"),
                          QStringLiteral("character"), QStringLiteral("user"));
        seeds << mkDrawer(QStringLiteral("Cenários"), QStringLiteral("#39d98a"),
                          QStringLiteral("setting"), QStringLiteral("map"));
        seeds << mkDrawer(QStringLiteral("Objetos"), QStringLiteral("#ff9f43"),
                          QStringLiteral("object"), QStringLiteral("cube"));
        seeds << mkDrawer(QStringLiteral("Notas"), QStringLiteral("#ffd84d"));
    } else if (templateId == QLatin1String("advanced")) {
        seeds << mkDrawer(QStringLiteral("Planejamento"), QStringLiteral("#2b79ff"),
                          QString(), QString(),
                          { mkItem(QStringLiteral("Sinopse")),
                            mkItem(QStringLiteral("Ambientação")),
                            mkItem(QStringLiteral("Fios narrativos")),
                            mkItem(QStringLiteral("Trama")),
                            mkItem(QStringLiteral("Fábula")),
                            mkItem(QStringLiteral("Roteiro")) });
        seeds << mkDrawer(QStringLiteral("Lore"), QStringLiteral("#9b59b6"));
        seeds << mkDrawer(QStringLiteral("Base de dados"), QStringLiteral("#1abc9c"));
        seeds << mkDrawer(QStringLiteral("Pesquisa"), QStringLiteral("#e67e22"));
        seeds << mkDrawer(QStringLiteral("Personagens"), QStringLiteral("#ff4d4d"),
                          QStringLiteral("character"), QStringLiteral("user"));
        seeds << mkDrawer(QStringLiteral("Cenários"), QStringLiteral("#39d98a"),
                          QStringLiteral("setting"), QStringLiteral("map"));
        seeds << mkDrawer(QStringLiteral("Objetos"), QStringLiteral("#ff9f43"),
                          QStringLiteral("object"), QStringLiteral("cube"));
        seeds << mkDrawer(QStringLiteral("Notas"), QStringLiteral("#ffd84d"));
    }
    // "blank" ou desconhecido → nenhum drawer.

    for (const Drawer& d : seeds) m_drawers.append(d);
    if (!seeds.isEmpty()) emit drawersChanged();
}

bool ProjectModel::addDrawerItem(const QString& drawerKey, const DrawerItem& item) {
    for (auto& d : m_drawers) {
        if (d.key == drawerKey) {
            d.items.append(item);
            emit drawersChanged();
            return true;
        }
    }
    return false;
}

bool ProjectModel::addDrawerFolder(const QString& drawerKey, const Folder& folder) {
    for (auto& d : m_drawers) {
        if (d.key == drawerKey) {
            d.folders.append(folder);
            emit drawersChanged();
            return true;
        }
    }
    return false;
}

bool ProjectModel::moveDrawerItem(const QString& drawerKey, const QString& itemId, const QString& newFolderId) {
    for (auto& d : m_drawers) {
        if (d.key != drawerKey) continue;
        for (auto& it : d.items) {
            if (it.id == itemId) {
                if (it.folderId == newFolderId) return true;
                it.folderId = newFolderId;
                emit drawersChanged();
                return true;
            }
        }
    }
    return false;
}

bool ProjectModel::moveDrawerItemToDrawer(const QString& srcDrawerKey, const QString& itemId,
                                          const QString& destDrawerKey, const QString& destFolderId) {
    if (srcDrawerKey == destDrawerKey) {
        return moveDrawerItem(srcDrawerKey, itemId, destFolderId);
    }
    int srcIdx = -1, destIdx = -1;
    for (int i = 0; i < m_drawers.size(); ++i) {
        if (m_drawers.at(i).key == srcDrawerKey)  srcIdx = i;
        if (m_drawers.at(i).key == destDrawerKey) destIdx = i;
    }
    if (srcIdx < 0 || destIdx < 0) return false;

    DrawerItem moved;
    bool found = false;
    {
        Drawer& src = m_drawers[srcIdx];
        for (int j = 0; j < src.items.size(); ++j) {
            if (src.items.at(j).id == itemId) {
                moved = src.items.at(j);
                src.items.removeAt(j);
                found = true;
                break;
            }
        }
    }
    if (!found) return false;

    moved.folderId = destFolderId;
    m_drawers[destIdx].items.append(moved);
    emit drawersChanged();
    return true;
}

bool ProjectModel::moveDrawerFolder(const QString& drawerKey, const QString& folderId, const QString& newParentId) {
    if (folderId == newParentId) return false; // não pode ser pai de si
    for (auto& d : m_drawers) {
        if (d.key != drawerKey) continue;
        for (auto& f : d.folders) {
            if (f.id == folderId) {
                if (f.parentId == newParentId) return true;
                f.parentId = newParentId;
                emit drawersChanged();
                return true;
            }
        }
    }
    return false;
}

const Drawer* ProjectModel::findDrawer(const QString& key) const {
    for (const auto& d : m_drawers) {
        if (d.key == key) return &d;
    }
    return nullptr;
}

bool ProjectModel::reorderDrawer(const QString& drawerKey, int targetIndex) {
    int srcIdx = -1;
    for (int i = 0; i < m_drawers.size(); ++i) {
        if (m_drawers.at(i).key == drawerKey) { srcIdx = i; break; }
    }
    if (srcIdx < 0) return false;
    if (targetIndex < 0) targetIndex = 0;
    if (targetIndex > m_drawers.size()) targetIndex = m_drawers.size();
    // Ajuste: ao remover antes do índice destino, o índice cai 1
    if (srcIdx < targetIndex) --targetIndex;
    if (srcIdx == targetIndex) return true;
    Drawer moved = m_drawers.takeAt(srcIdx);
    m_drawers.insert(targetIndex, moved);
    emit drawersChanged();
    return true;
}

bool ProjectModel::reorderDrawerItem(const QString& drawerKey, const QString& itemId, int targetIndex) {
    for (auto& d : m_drawers) {
        if (d.key != drawerKey) continue;
        int srcIdx = -1;
        for (int i = 0; i < d.items.size(); ++i) {
            if (d.items.at(i).id == itemId) { srcIdx = i; break; }
        }
        if (srcIdx < 0) return false;
        if (targetIndex < 0) targetIndex = 0;
        if (targetIndex > d.items.size()) targetIndex = d.items.size();
        if (srcIdx < targetIndex) --targetIndex;
        if (srcIdx == targetIndex) return true;
        DrawerItem moved = d.items.takeAt(srcIdx);
        d.items.insert(targetIndex, moved);
        emit drawersChanged();
        return true;
    }
    return false;
}

bool ProjectModel::removeDrawerItem(const QString& itemId) {
    for (auto& d : m_drawers) {
        for (int i = 0; i < d.items.size(); ++i) {
            if (d.items.at(i).id == itemId) {
                d.items.removeAt(i);
                emit drawersChanged();
                return true;
            }
        }
    }
    return false;
}

bool ProjectModel::removeDrawerFolder(const QString& drawerKey, const QString& folderId) {
    for (auto& d : m_drawers) {
        if (d.key != drawerKey) continue;
        // Bloqueia se tiver items ou subpastas dentro
        for (const auto& it : d.items) {
            if (it.folderId == folderId) return false;
        }
        for (const auto& f : d.folders) {
            if (f.parentId == folderId) return false;
        }
        for (int i = 0; i < d.folders.size(); ++i) {
            if (d.folders.at(i).id == folderId) {
                d.folders.removeAt(i);
                emit drawersChanged();
                return true;
            }
        }
    }
    return false;
}

bool ProjectModel::removeDrawer(const QString& drawerKey) {
    for (int i = 0; i < m_drawers.size(); ++i) {
        if (m_drawers.at(i).key != drawerKey) continue;
        // Só permite remover se vazia (sem items e sem folders)
        if (!m_drawers.at(i).items.isEmpty() || !m_drawers.at(i).folders.isEmpty()) return false;
        m_drawers.removeAt(i);
        emit drawersChanged();
        return true;
    }
    return false;
}

bool ProjectModel::drawerIsEmpty(const QString& drawerKey) const {
    const Drawer* d = findDrawer(drawerKey);
    if (!d) return true;
    return d->items.isEmpty() && d->folders.isEmpty();
}

bool ProjectModel::updateDrawer(const QString& drawerKey, const QString& title, const QString& color,
                                const QString& iconId, const QString& elementType, const QString& elementIcon) {
    for (auto& d : m_drawers) {
        if (d.key != drawerKey) continue;
        bool changed = false;
        if (!title.isNull() && d.title != title) { d.title = title; changed = true; }
        if (!color.isNull() && d.color != color) { d.color = color; changed = true; }
        if (!iconId.isNull() && d.drawerIcon != iconId) { d.drawerIcon = iconId; changed = true; }
        if (!elementType.isNull() && d.drawerElementType != elementType) {
            d.drawerElementType = elementType;
            changed = true;
        }
        if (!elementIcon.isNull() && d.drawerElementIcon != elementIcon) {
            d.drawerElementIcon = elementIcon;
            changed = true;
        }
        if (changed) emit drawersChanged();
        return true;
    }
    return false;
}

bool ProjectModel::updateDrawerItemHtml(const QString& itemId, const QString& html) {
    for (auto& d : m_drawers) {
        for (auto& it : d.items) {
            if (it.id != itemId) continue;
            if (!it.file.isEmpty()) return false; // file-backed não muda inline
            if (it.html == html && it.hasInlineHtml) return true;
            it.html = html;
            it.hasInlineHtml = true;
            return true;
        }
    }
    return false;
}

bool ProjectModel::updateDrawerItemMeta(const QString& itemId, const QString& title, const QString& role) {
    for (auto& d : m_drawers) {
        for (auto& it : d.items) {
            if (it.id != itemId) continue;
            bool changed = false;
            if (!title.isNull() && it.title != title) { it.title = title; changed = true; }
            if (it.role != role) { it.role = role; changed = true; }
            if (changed) emit drawersChanged();
            return true;
        }
    }
    return false;
}

bool ProjectModel::updateDrawerItemConsistency(const QString& itemId, const QString& status,
                                               const QString& statusDetail, const QString& location) {
    for (auto& d : m_drawers) {
        for (auto& it : d.items) {
            if (it.id != itemId) continue;
            bool changed = false;
            if (it.charStatus != status)             { it.charStatus = status;             changed = true; }
            if (it.charStatusDetail != statusDetail) { it.charStatusDetail = statusDetail; changed = true; }
            if (it.charLocation != location)         { it.charLocation = location;         changed = true; }
            if (changed) emit drawersChanged();
            return true;
        }
    }
    return false;
}

bool ProjectModel::setDrawerItemElement(const QString& itemId, const QString& elementType,
                                        const QString& elementIcon, const QString& elementId) {
    for (auto& d : m_drawers) {
        for (auto& it : d.items) {
            if (it.id != itemId) continue;
            bool changed = false;
            if (it.elementType != elementType) { it.elementType = elementType; changed = true; }
            if (it.elementIcon != elementIcon) { it.elementIcon = elementIcon; changed = true; }
            if (it.elementId != elementId)     { it.elementId = elementId;     changed = true; }
            if (changed) emit drawersChanged();
            return true;
        }
    }
    return false;
}

const DrawerItem* ProjectModel::findDrawerItem(const QString& itemId, QString* outDrawerKey) const {
    for (const auto& d : m_drawers) {
        for (const auto& it : d.items) {
            if (it.id == itemId) {
                if (outDrawerKey) *outDrawerKey = d.key;
                return &it;
            }
        }
    }
    return nullptr;
}

QList<CharacterBond> ProjectModel::characterBondsForDrawer(const QString& drawerKey) const {
    QList<CharacterBond> out;
    for (const auto& b : m_characterBonds) {
        if (b.drawerKey == drawerKey) out.append(b);
    }
    return out;
}

QString ProjectModel::addCharacterBond(const QString& drawerKey, const QString& fromItemId,
                                       const QString& toItemId, const QString& type,
                                       const QString& description, const QString& color) {
    if (drawerKey.isEmpty() || fromItemId.isEmpty() || toItemId.isEmpty()) return QString();
    if (fromItemId == toItemId) return QString();
    // Não duplica: se já existe um bond ligando esses dois itens nesta gaveta
    // (independente da direção), retorna o id existente.
    for (const auto& b : m_characterBonds) {
        if (b.drawerKey != drawerKey) continue;
        if ((b.fromItemId == fromItemId && b.toItemId == toItemId) ||
            (b.fromItemId == toItemId && b.toItemId == fromItemId)) {
            return b.id;
        }
    }
    CharacterBond b;
    b.id = ProjectModel::uid();
    b.drawerKey = drawerKey;
    b.fromItemId = fromItemId;
    b.toItemId = toItemId;
    b.type = type;
    b.description = description;
    b.color = color.isEmpty() ? QStringLiteral("#4a9eff") : color;
    m_characterBonds.append(b);
    emit characterBondsChanged();
    return b.id;
}

bool ProjectModel::updateCharacterBond(const QString& bondId, const QString& type,
                                       const QString& description, const QString& color) {
    for (auto& b : m_characterBonds) {
        if (b.id != bondId) continue;
        bool changed = false;
        if (b.type != type) { b.type = type; changed = true; }
        if (b.description != description) { b.description = description; changed = true; }
        const QString newColor = color.isEmpty() ? b.color : color;
        if (b.color != newColor) { b.color = newColor; changed = true; }
        if (changed) emit characterBondsChanged();
        return true;
    }
    return false;
}

bool ProjectModel::removeCharacterBond(const QString& bondId) {
    for (int i = 0; i < m_characterBonds.size(); ++i) {
        if (m_characterBonds.at(i).id == bondId) {
            m_characterBonds.removeAt(i);
            emit characterBondsChanged();
            return true;
        }
    }
    return false;
}

void ProjectModel::removeBondsForItem(const QString& itemId) {
    const int before = m_characterBonds.size();
    m_characterBonds.erase(std::remove_if(m_characterBonds.begin(), m_characterBonds.end(),
        [&](const CharacterBond& b) { return b.fromItemId == itemId || b.toItemId == itemId; }),
        m_characterBonds.end());
    if (m_characterBonds.size() != before) emit characterBondsChanged();
}

void ProjectModel::removeBondsForDrawer(const QString& drawerKey) {
    const int before = m_characterBonds.size();
    m_characterBonds.erase(std::remove_if(m_characterBonds.begin(), m_characterBonds.end(),
        [&](const CharacterBond& b) { return b.drawerKey == drawerKey; }),
        m_characterBonds.end());
    if (m_characterBonds.size() != before) emit characterBondsChanged();
}

const CharacterBond* ProjectModel::findCharacterBond(const QString& bondId) const {
    for (const auto& b : m_characterBonds) {
        if (b.id == bondId) return &b;
    }
    return nullptr;
}

void ProjectModel::addManuscript(const Manuscript& manuscript) {
    m_manuscripts.append(manuscript);
    emit manuscriptsChanged();
}

void ProjectModel::addChapter(const Chapter& chapter) {
    Chapter c = chapter;
    if (c.file.isEmpty() && !c.id.isEmpty()) {
        c.file = chapterDefaultFile(c.manuscriptId, c.id);
    }
    if (c.order == 0) {
        int maxOrder = 0;
        for (const auto& existing : m_chapters) {
            if (existing.manuscriptId == c.manuscriptId && existing.order > maxOrder) {
                maxOrder = existing.order;
            }
        }
        c.order = maxOrder + 1;
    }
    m_chapters.append(c);
    emit chaptersChanged();
}

bool ProjectModel::updateChapterScenes(const QString& chapterId, const QList<Scene>& scenes) {
    for (auto& c : m_chapters) {
        if (c.id == chapterId) {
            if (c.scenes.size() == scenes.size()) {
                bool same = true;
                for (int i = 0; i < scenes.size(); ++i) {
                    if (c.scenes[i].id != scenes[i].id || c.scenes[i].order != scenes[i].order) {
                        same = false; break;
                    }
                }
                if (same) return true;
            }
            c.scenes = scenes;
            emit chaptersChanged();
            return true;
        }
    }
    return false;
}

bool ProjectModel::updateChapterTitle(const QString& chapterId, const QString& title) {
    for (auto& c : m_chapters) {
        if (c.id != chapterId) continue;
        if (c.title == title) return true;
        c.title = title;
        emit chaptersChanged();
        return true;
    }
    return false;
}

bool ProjectModel::removeChapter(const QString& chapterId) {
    for (int i = 0; i < m_chapters.size(); ++i) {
        if (m_chapters.at(i).id != chapterId) continue;
        const QString msId = m_chapters.at(i).manuscriptId;
        m_chapters.removeAt(i);
        // Re-numerar a ordem dos capítulos restantes do mesmo manuscrito.
        int n = 1;
        for (auto& c : m_chapters) {
            if (c.manuscriptId == msId) { c.order = n++; }
        }
        if (m_activeChapterId == chapterId) m_activeChapterId.clear();
        emit chaptersChanged();
        return true;
    }
    return false;
}

bool ProjectModel::reorderChapter(const QString& chapterId, int targetIndex) {
    // Reordena entre os capítulos do mesmo manuscrito.
    int srcIdx = -1;
    QString msId;
    for (int i = 0; i < m_chapters.size(); ++i) {
        if (m_chapters.at(i).id == chapterId) {
            srcIdx = i;
            msId = m_chapters.at(i).manuscriptId;
            break;
        }
    }
    if (srcIdx < 0) return false;

    // Lista de índices globais dos capítulos do manuscrito, na ordem atual.
    QList<int> indicesInMs;
    for (int i = 0; i < m_chapters.size(); ++i) {
        if (m_chapters.at(i).manuscriptId == msId) indicesInMs.append(i);
    }
    const int posInMs = indicesInMs.indexOf(srcIdx);
    if (posInMs < 0) return false;

    if (targetIndex < 0) targetIndex = 0;
    if (targetIndex > indicesInMs.size()) targetIndex = indicesInMs.size();
    if (posInMs < targetIndex) --targetIndex;
    if (posInMs == targetIndex) return true;

    Chapter moved = m_chapters.takeAt(srcIdx);
    // Após remover, recomputa onde inserir no array global.
    int globalInsertPos;
    if (targetIndex >= indicesInMs.size() - 1) {
        // Inserir depois do último cap do manuscrito.
        int lastIdx = -1;
        for (int i = 0; i < m_chapters.size(); ++i) {
            if (m_chapters.at(i).manuscriptId == msId) lastIdx = i;
        }
        globalInsertPos = lastIdx + 1;
    } else {
        // Pegar o targetIndex-ésimo cap do manuscrito no array recalculado.
        int seen = 0;
        globalInsertPos = m_chapters.size();
        for (int i = 0; i < m_chapters.size(); ++i) {
            if (m_chapters.at(i).manuscriptId == msId) {
                if (seen == targetIndex) { globalInsertPos = i; break; }
                ++seen;
            }
        }
    }
    m_chapters.insert(globalInsertPos, moved);

    // Re-numerar a ordem dos capítulos do manuscrito.
    int n = 1;
    for (auto& c : m_chapters) {
        if (c.manuscriptId == msId) c.order = n++;
    }
    emit chaptersChanged();
    return true;
}

bool ProjectModel::updateSceneTitle(const QString& chapterId, int sceneIndex, const QString& title) {
    for (auto& c : m_chapters) {
        if (c.id != chapterId) continue;
        if (sceneIndex < 0 || sceneIndex >= c.scenes.size()) return false;
        if (c.scenes[sceneIndex].title == title) return true;
        c.scenes[sceneIndex].title = title;
        emit chaptersChanged();
        return true;
    }
    return false;
}

const Chapter* ProjectModel::findChapter(const QString& chapterId) const {
    for (const auto& c : m_chapters) {
        if (c.id == chapterId) return &c;
    }
    return nullptr;
}

const Scene* ProjectModel::findScene(const QString& chapterId, int sceneIndex) const {
    const Chapter* ch = findChapter(chapterId);
    if (!ch || sceneIndex < 0 || sceneIndex >= ch->scenes.size()) return nullptr;
    return &ch->scenes.at(sceneIndex);
}

QString ProjectModel::createSceneVariation(const QString& chapterId, int sceneIndex, const QString& label) {
    for (auto& c : m_chapters) {
        if (c.id != chapterId) continue;
        if (sceneIndex < 0 || sceneIndex >= c.scenes.size()) return QString();
        Scene& scene = c.scenes[sceneIndex];

        Variation v;
        v.id = ProjectModel::uid();
        v.label = label.isEmpty() ? QStringLiteral("Variação %1").arg(scene.variations.size() + 1) : label;
        v.isPrimary = scene.variations.isEmpty();
        scene.variations.append(v);
        if (scene.activeVariationId.isEmpty() || scene.variations.size() == 1) {
            scene.activeVariationId = v.id;
        }
        emit chaptersChanged();
        return v.id;
    }
    return QString();
}

bool ProjectModel::switchActiveVariation(const QString& chapterId, int sceneIndex, const QString& variationId) {
    for (auto& c : m_chapters) {
        if (c.id != chapterId) continue;
        if (sceneIndex < 0 || sceneIndex >= c.scenes.size()) return false;
        Scene& scene = c.scenes[sceneIndex];
        for (const auto& v : scene.variations) {
            if (v.id == variationId) {
                if (scene.activeVariationId == variationId) return true;
                scene.activeVariationId = variationId;
                emit chaptersChanged();
                return true;
            }
        }
        return false;
    }
    return false;
}

bool ProjectModel::setPrimaryVariation(const QString& chapterId, int sceneIndex, const QString& variationId) {
    for (auto& c : m_chapters) {
        if (c.id != chapterId) continue;
        if (sceneIndex < 0 || sceneIndex >= c.scenes.size()) return false;
        Scene& scene = c.scenes[sceneIndex];
        bool found = false;
        for (auto& v : scene.variations) {
            v.isPrimary = (v.id == variationId);
            if (v.isPrimary) found = true;
        }
        if (found) emit chaptersChanged();
        return found;
    }
    return false;
}

bool ProjectModel::removeVariation(const QString& chapterId, int sceneIndex, const QString& variationId) {
    for (auto& c : m_chapters) {
        if (c.id != chapterId) continue;
        if (sceneIndex < 0 || sceneIndex >= c.scenes.size()) return false;
        Scene& scene = c.scenes[sceneIndex];
        if (scene.variations.size() <= 1) return false; // não permite remover a única
        const bool wasActive = (scene.activeVariationId == variationId);
        const bool wasPrimary = std::any_of(scene.variations.cbegin(), scene.variations.cend(),
            [&](const Variation& v) { return v.id == variationId && v.isPrimary; });
        const int before = scene.variations.size();
        scene.variations.erase(std::remove_if(scene.variations.begin(), scene.variations.end(),
            [&](const Variation& v) { return v.id == variationId; }), scene.variations.end());
        if (scene.variations.size() == before) return false;

        if (wasActive) scene.activeVariationId = scene.variations.first().id;
        if (wasPrimary) scene.variations.first().isPrimary = true;
        emit chaptersChanged();
        return true;
    }
    return false;
}

QList<Scene> ProjectModel::buildScenesFromHtml(const QString& html, const QList<Scene>& existing) {
    const int segmentCount = SceneUtils::countSceneDelimiters(html) + 1;
    if (segmentCount <= 1) return {};
    QList<Scene> scenes;
    scenes.reserve(segmentCount);
    for (int i = 0; i < segmentCount; ++i) {
        if (i < existing.size()) {
            Scene s = existing.at(i);
            s.order = i;
            scenes.append(s);
        } else {
            Scene s;
            s.id = ProjectModel::uid();
            s.title = QStringLiteral("Cena %1").arg(i + 1);
            s.order = i;
            scenes.append(s);
        }
    }
    return scenes;
}

QJsonObject ProjectModel::toJson() const {
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("projectName"), m_projectName.isEmpty() ? QStringLiteral("Projeto") : m_projectName);

    QJsonArray chapters;
    int idx = 0;
    for (const auto& c : m_chapters) {
        chapters.append(chapterToJson(c, idx + 1));
        ++idx;
    }
    root.insert(QStringLiteral("chapters"), chapters);

    QJsonArray drawers;
    for (const auto& d : m_drawers) drawers.append(drawerToJson(d));
    root.insert(QStringLiteral("drawers"), drawers);

    root.insert(QStringLiteral("settings"), m_settings);
    root.insert(QStringLiteral("ui"), m_ui);

    QJsonObject data = m_dataExtras;
    QJsonArray manuscripts;
    for (const auto& m : m_manuscripts) manuscripts.append(manuscriptToJson(m));
    data.insert(QStringLiteral("manuscripts"), manuscripts);
    data.insert(QStringLiteral("activeManuscriptId"), stringOrNull(m_activeManuscriptId));
    data.insert(QStringLiteral("activeChapterId"), stringOrNull(m_activeChapterId));

    // Vínculos entre personagens — compat 1:1 com Mira 1 (data.characterBonds,
    // array plano com 7 campos por bond).
    QJsonArray bonds;
    for (const auto& b : m_characterBonds) bonds.append(characterBondToJson(b));
    data.insert(QStringLiteral("characterBonds"), bonds);

    root.insert(QStringLiteral("data"), data);

    return root;
}

void ProjectModel::loadFromJson(const QJsonObject& root) {
    m_projectName = jsonString(root.value(QStringLiteral("projectName")));

    m_chapters.clear();
    const QJsonArray chapters = root.value(QStringLiteral("chapters")).toArray();
    for (const auto& cv : chapters) m_chapters.append(chapterFromJson(cv.toObject()));

    m_drawers.clear();
    const QJsonArray drawers = root.value(QStringLiteral("drawers")).toArray();
    for (const auto& dv : drawers) m_drawers.append(drawerFromJson(dv.toObject()));

    m_settings = root.value(QStringLiteral("settings")).toObject();
    m_ui = root.value(QStringLiteral("ui")).toObject();

    QJsonObject data = root.value(QStringLiteral("data")).toObject();
    m_manuscripts.clear();
    const QJsonArray manuscripts = data.value(QStringLiteral("manuscripts")).toArray();
    for (const auto& mv : manuscripts) m_manuscripts.append(manuscriptFromJson(mv.toObject()));
    m_activeManuscriptId = jsonStringOrEmpty(data.value(QStringLiteral("activeManuscriptId")));
    m_activeChapterId = jsonStringOrEmpty(data.value(QStringLiteral("activeChapterId")));

    m_characterBonds.clear();
    const QJsonArray bonds = data.value(QStringLiteral("characterBonds")).toArray();
    for (const auto& bv : bonds) m_characterBonds.append(characterBondFromJson(bv.toObject()));

    QJsonObject extras = data;
    extras.remove(QStringLiteral("manuscripts"));
    extras.remove(QStringLiteral("activeManuscriptId"));
    extras.remove(QStringLiteral("activeChapterId"));
    extras.remove(QStringLiteral("characterBonds"));
    m_dataExtras = extras;

    emit projectNameChanged();
    emit manuscriptsChanged();
    emit chaptersChanged();
    emit drawersChanged();
    emit activeManuscriptChanged();
    emit activeChapterChanged();
    emit settingsChanged();
    emit uiChanged();
    emit characterBondsChanged();
    emit loaded();
}

void ProjectModel::clear() {
    m_projectName.clear();
    m_manuscripts.clear();
    m_chapters.clear();
    m_drawers.clear();
    m_characterBonds.clear();
    m_activeManuscriptId.clear();
    m_activeChapterId.clear();
    m_settings = QJsonObject();
    m_ui = QJsonObject();
    m_dataExtras = QJsonObject();
    emit projectNameChanged();
    emit manuscriptsChanged();
    emit chaptersChanged();
    emit drawersChanged();
    emit activeManuscriptChanged();
    emit activeChapterChanged();
    emit settingsChanged();
    emit uiChanged();
    emit characterBondsChanged();
}

QString ProjectModel::uid() {
    static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    constexpr int alen = sizeof(alphabet) - 1;
    QString out;
    out.reserve(9);
    auto* rng = QRandomGenerator::global();
    for (int i = 0; i < 9; ++i) {
        out.append(QLatin1Char(alphabet[rng->bounded(alen)]));
    }
    return out;
}

QString ProjectModel::chapterDefaultFile(const QString& manuscriptId, const QString& chapterId) {
    if (!manuscriptId.isEmpty()) {
        return QStringLiteral("content/manuscripts/ms_%1/chapters/ch_%2.html")
            .arg(manuscriptId, chapterId);
    }
    return QStringLiteral("content/chapters/ch_%1.html").arg(chapterId);
}
