#pragma once

#include "DocCache.h"

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

struct ElementType {
    QString id;       // "character" | "setting" | "object"
    QString label;
    QString color;
    QString icon;
};

struct Element {
    QString id;
    QString name;
    QString type;             // matches ElementType::id
    QString icon;
    QString image;            // data URL JPEG base64
    QString role;             // PROTAGONISTA, COADJUVANTE, etc. (Mira 2 extension)
    QString trackMode;        // "" = auto por role | "on" = sempre | "off" = nunca (trilha da linha do tempo)
    bool narrator = false;
    QString textConcordance;  // letra única usada na detecção (compat Mira 1)
    QStringList aliases;
};

// Gerencia elements.json separado do project.mira.json.
// Espelha o shape do Mira 1: elements[], elementTypes[], docElements{}.
// Imagens base64 ficam aqui, não no project index — performance + arquitetura limpa.
class ElementsStore : public QObject {
    Q_OBJECT
public:
    explicit ElementsStore(QObject* parent = nullptr);

    void setProjectRoot(const QString& root);
    QString projectRoot() const { return m_root; }

    bool load();   // lê elements.json (se existir) e popula state
    bool save();   // grava elements.json se dirty
    void clear();  // estado vazio

    bool isDirty() const { return m_dirty; }

    // Elementos
    QList<Element> elements() const;
    const Element* findElement(const QString& id) const;
    QString addElement(Element e); // retorna id (gera se vazio)
    bool updateElement(const QString& id, const Element& e);
    bool removeElement(const QString& id);

    // Tipos (definição estática, mas exposta pra UI/extensão futura)
    QList<ElementType> elementTypes() const { return m_elementTypes; }
    const ElementType* findType(const QString& id) const;

    // docElements: chapter/scene key -> [elementId, ...] (presença detectada)
    QJsonObject docElements() const { return m_docElements; }
    QStringList docElementIds(const QString& docKey) const;
    bool hasDocElement(const QString& docKey, const QString& elementId) const;
    void addDocElement(const QString& docKey, const QString& elementId);
    void addManyDocElements(const QString& docKey, const QStringList& elementIds);
    void removeDocElement(const QString& docKey, const QString& elementId);

    // Chaves de docKey usadas pela marcação manual de "elementos presentes".
    // Capítulo reaproveita DocCache::chapterKey (mesma chave da detecção
    // automática — união natural entre auto-detecção e marcação manual no
    // nível do capítulo). Cena é granularidade nova, exclusiva desta feature,
    // já que cenas vivem dentro do HTML do capítulo e não têm chave própria
    // no DocCache/EditorHost.
    static QString elementDocKeyForChapter(const QString& manuscriptId, const QString& chapterId);
    static QString elementDocKeyForScene(const QString& manuscriptId, const QString& chapterId,
                                          const QString& sceneId);

signals:
    void changed();

private:
    void seedDefaultTypes();
    QJsonObject toJson() const;
    void loadFromJson(const QJsonObject& root);

    QString m_root;
    QHash<QString, Element> m_elements; // id -> element
    QList<QString> m_elementOrder;      // preserva ordem de inserção
    QList<ElementType> m_elementTypes;
    QJsonObject m_docElements;
    bool m_dirty = false;
};
