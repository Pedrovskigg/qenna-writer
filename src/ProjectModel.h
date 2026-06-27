#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>

struct Variation {
    QString id;
    QString label;
    bool isPrimary = false;
};

struct Scene {
    QString id;
    QString title;
    int order = 0;
    QList<Variation> variations;
    QString activeVariationId;
    QString timeMarker;   // "quando se passa" (tempo da história) — Mira 2
};

struct Chapter {
    QString id;
    QString manuscriptId;
    QString title;
    QString file;
    int order = 0;
    QList<Scene> scenes;
    QString timeMarker;   // "quando se passa" (tempo da história) — Mira 2
};

struct Manuscript {
    QString id;
    QString title;
    QString html;
};

struct Group {
    QString id;
    QString title;
    QString color;
};

struct Folder {
    QString id;
    QString title;
    QString parentId;
    QString markerId;
};

// Campo de uma ficha de personagem estruturada (formulário). Ver CharacterSheet.
struct SheetField {
    QString id;
    QString label;                            // rótulo exibido em negrito
    QString kind = QStringLiteral("text");    // "data" (linha curta) | "text" (área rica/html)
    QString value;                            // texto puro p/ "data"; html p/ "text"
    QString column = QStringLiteral("left");  // "left" | "right" (distribuição entre colunas)
    int order = 0;                            // ordem dentro da coluna
};

// Ficha estruturada de personagem: alternativa ao documento livre (html).
// Foto/nome/apelido NÃO ficam aqui — vêm do Element vinculado (elementId).
struct CharacterSheet {
    int columns = 2;            // layout: 1 ou 2 colunas
    QList<SheetField> fields;
    bool isEmpty() const { return fields.isEmpty(); }
};

struct DrawerItem {
    QString id;
    QString title;
    QString folderId;
    QString markerId;
    QString elementId;
    QString elementType;
    QString elementIcon;
    QString file;
    QString html;
    bool hasInlineHtml = false;
    bool isChapter = false;
    QString chapterRef;
    QString role;
    // Campos de consistência narrativa (só usados em gavetas de personagem)
    QString charStatus;
    QString charStatusDetail;
    QString charLocation;
    // Ficha estruturada (formulário). Quando isSheet=true, o documento é a ficha
    // (sheet) em vez do html livre.
    bool isSheet = false;
    CharacterSheet sheet;
};

struct Drawer {
    QString key;
    QString title;
    QString color;
    QString drawerIcon;
    QString drawerElementType;
    QString drawerElementIcon;
    QList<Folder> folders;
    QList<DrawerItem> items;
};

struct CharacterBond {
    QString id;
    QString drawerKey;
    QString fromItemId;
    QString toItemId;
    QString type;
    QString description;
    QString color;
};

class ProjectModel : public QObject {
    Q_OBJECT
public:
    explicit ProjectModel(QObject* parent = nullptr);

    QString projectName() const { return m_projectName; }
    const QList<Manuscript>& manuscripts() const { return m_manuscripts; }
    const QList<Chapter>& chapters() const { return m_chapters; }
    const QList<Drawer>& drawers() const { return m_drawers; }
    QString activeManuscriptId() const { return m_activeManuscriptId; }
    QString activeChapterId() const { return m_activeChapterId; }
    QJsonObject settings() const { return m_settings; }
    QJsonObject ui() const { return m_ui; }
    QJsonObject dataExtras() const { return m_dataExtras; }

    bool firstLineIndentEnabled() const;
    void setFirstLineIndentEnabled(bool enabled);

    int paragraphSpacingBefore() const;
    void setParagraphSpacingBefore(int px);

    int paragraphSpacingAfter() const;
    void setParagraphSpacingAfter(int px);

    int lineHeightPercent() const;
    void setLineHeightPercent(int percent);

    // "" = corretor desligado; "pt_BR", "en_US", ... = idioma do dicionário.
    QString spellLanguage() const;
    void setSpellLanguage(const QString& code);

    void setProjectName(const QString& name);
    void setManuscripts(const QList<Manuscript>& list);
    void setChapters(const QList<Chapter>& list);
    void setDrawers(const QList<Drawer>& list);
    void setActiveManuscriptId(const QString& id);
    void setActiveChapterId(const QString& id);
    void setGroups(const QList<Group>& list);
    void setSettings(const QJsonObject& s);
    void setUi(const QJsonObject& u);
    void setDataExtras(const QJsonObject& d);

    // Semeia drawers iniciais do projeto a partir de um template. Compat com
    // os templates do Mira 1 (buildTemplateDrawers em App.jsx).
    // templateId: "blank" | "basic" | "advanced".
    void seedFromTemplate(const QString& templateId);

    void addDrawer(const Drawer& drawer);
    bool addDrawerItem(const QString& drawerKey, const DrawerItem& item);
    bool addDrawerFolder(const QString& drawerKey, const Folder& folder);
    bool moveDrawerItem(const QString& drawerKey, const QString& itemId, const QString& newFolderId);
    bool moveDrawerItemToDrawer(const QString& srcDrawerKey, const QString& itemId,
                                const QString& destDrawerKey, const QString& destFolderId);
    bool moveDrawerFolder(const QString& drawerKey, const QString& folderId, const QString& newParentId);
    bool reorderDrawer(const QString& drawerKey, int targetIndex);
    bool reorderDrawerItem(const QString& drawerKey, const QString& itemId, int targetIndex);
    bool removeDrawerItem(const QString& itemId);
    bool removeDrawerFolder(const QString& drawerKey, const QString& folderId);
    bool removeDrawer(const QString& drawerKey);
    bool drawerIsEmpty(const QString& drawerKey) const;
    bool updateDrawer(const QString& drawerKey, const QString& title, const QString& color,
                      const QString& iconId, const QString& elementType, const QString& elementIcon);
    const Drawer* findDrawer(const QString& key) const;

    // Detalhes da obra (autor, gêneros, sinopse, capa). Compat com Mira 1:
    // ficam em data.projectDetails no JSON, preservados pelo m_dataExtras.
    QString projectAuthor() const;
    QString projectGenres() const;
    QString projectSynopsis() const;
    QString projectCoverDataUrl() const;
    void setProjectDetails(const QString& name, const QString& author,
                           const QString& genres, const QString& synopsis,
                           const QString& coverDataUrl);

    // Tipo do projeto: "book" (padrão) ou "screenplay".
    QString projectType() const { return m_projectType; }
    bool isScreenplay() const { return m_projectType == QStringLiteral("screenplay"); }
    void setProjectType(const QString& type);

    // Alinhamento padrão por escopo (int = Qt::AlignmentFlag value).
    // 0 = não definido (usa Left como fallback no editor).
    int defaultManuscriptAlignment() const;
    void setDefaultManuscriptAlignment(int alignment);
    int defaultDrawerAlignment() const;
    void setDefaultDrawerAlignment(int alignment);

    bool updateDrawerItemHtml(const QString& itemId, const QString& html);
    bool updateDrawerItemSheet(const QString& itemId, const CharacterSheet& sheet);
    bool updateDrawerItemMeta(const QString& itemId, const QString& title, const QString& role);
    bool setDrawerItemElement(const QString& itemId, const QString& elementType,
                              const QString& elementIcon, const QString& elementId);
    bool updateDrawerItemConsistency(const QString& itemId, const QString& status,
                                     const QString& statusDetail, const QString& location);
    const DrawerItem* findDrawerItem(const QString& itemId, QString* outDrawerKey = nullptr) const;

    const QList<Group>& groups() const { return m_groups; }
    const Group* findGroup(const QString& id) const;
    QString addGroup(const QString& title, const QString& color);
    bool updateGroup(const QString& id, const QString& title, const QString& color);
    bool removeGroup(const QString& id);
    bool setDrawerItemGroup(const QString& itemId, const QString& groupId);
    bool setDrawerFolderGroup(const QString& drawerKey, const QString& folderId, const QString& groupId);

    const QList<CharacterBond>& characterBonds() const { return m_characterBonds; }
    QList<CharacterBond> characterBondsForDrawer(const QString& drawerKey) const;
    QString addCharacterBond(const QString& drawerKey, const QString& fromItemId,
                             const QString& toItemId, const QString& type,
                             const QString& description, const QString& color);
    bool updateCharacterBond(const QString& bondId, const QString& type,
                             const QString& description, const QString& color);
    bool removeCharacterBond(const QString& bondId);
    void removeBondsForItem(const QString& itemId);
    void removeBondsForDrawer(const QString& drawerKey);
    const CharacterBond* findCharacterBond(const QString& bondId) const;

    void addManuscript(const Manuscript& manuscript);
    void addChapter(const Chapter& chapter);
    bool updateChapterScenes(const QString& chapterId, const QList<Scene>& scenes);
    bool updateChapterTitle(const QString& chapterId, const QString& title);
    bool updateChapterTimeMarker(const QString& chapterId, const QString& marker);
    bool removeChapter(const QString& chapterId);
    bool reorderChapter(const QString& chapterId, int targetIndex);
    bool updateSceneTitle(const QString& chapterId, int sceneIndex, const QString& title);
    const Chapter* findChapter(const QString& chapterId) const;
    const Scene* findScene(const QString& chapterId, int sceneIndex) const;

    QString createSceneVariation(const QString& chapterId, int sceneIndex, const QString& label);
    bool switchActiveVariation(const QString& chapterId, int sceneIndex, const QString& variationId);
    bool setPrimaryVariation(const QString& chapterId, int sceneIndex, const QString& variationId);
    bool removeVariation(const QString& chapterId, int sceneIndex, const QString& variationId);

    static QList<Scene> buildScenesFromHtml(const QString& html, const QList<Scene>& existing);

    QJsonObject toJson() const;
    void loadFromJson(const QJsonObject& root);
    void clear();

    static QString uid();
    static QString chapterDefaultFile(const QString& manuscriptId, const QString& chapterId);

    // Ficha padrão de personagem: campos básicos (esquerda) + blocos narrativos (direita).
    static CharacterSheet defaultCharacterSheet();

    // Renderiza uma ficha como HTML legível (para RefMenu, exportação, busca).
    // name/aliases/imageDataUrl podem vir vazios; campos seguem a ordem da ficha.
    static QString characterSheetToHtml(const CharacterSheet& sheet, const QString& name,
                                        const QString& aliases, const QString& imageDataUrl);

signals:
    void projectNameChanged();
    void manuscriptsChanged();
    void chaptersChanged();
    void drawersChanged();
    void groupsChanged();
    void characterBondsChanged();
    void activeManuscriptChanged();
    void activeChapterChanged();
    void settingsChanged();
    void uiChanged();
    void projectDetailsChanged();
    void loaded();

private:
    QString m_projectName;
    QString m_projectType = QStringLiteral("book");
    QList<Manuscript> m_manuscripts;
    QList<Chapter> m_chapters;
    QList<Drawer> m_drawers;
    QString m_activeManuscriptId;
    QString m_activeChapterId;
    QJsonObject m_settings;
    QJsonObject m_ui;
    QJsonObject m_dataExtras;
    QList<Group> m_groups;
    QList<CharacterBond> m_characterBonds;
};
