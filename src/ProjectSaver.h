#pragma once

#include <QObject>
#include <QString>

class QTimer;
class ProjectModel;
class DocCache;
class EditorHost;
class ElementsStore;
class WordCounter;

// Orquestra persistência: paulada de saveProject (autosave/Ctrl+S), debounce
// por-doc para writes incrementais, lock pra evitar saves concorrentes.
class ProjectSaver : public QObject {
    Q_OBJECT
public:
    ProjectSaver(ProjectModel* model, DocCache* cache, EditorHost* host, QObject* parent = nullptr);

    void setElementsStore(ElementsStore* store) { m_elementsStore = store; }
    // Usado só pra cachear o total de palavras do projeto no índice, a cada
    // save — é o que a Prateleira 3D lê pra dar a espessura real da lombada
    // (em vez de adivinhar pelo tamanho do nome).
    void setWordCounter(WordCounter* counter) { m_wordCounter = counter; }
    void setProjectRoot(const QString& root);
    QString projectRoot() const { return m_root; }

    // 0 desativa autosave.
    void setAutosaveIntervalMs(int ms);
    void setPerDocDebounceMs(int ms);

    bool isSaving() const { return m_saving; }
    bool hasDirtyContent() const;
    QString lastError() const { return m_lastError; }

public slots:
    // Paulada completa: editor->cache, todos os dirty no disco, escreve index.
    // Retorna true se gravou sem erros (ou se não havia nada pra salvar).
    bool saveProject();

    // Grava apenas os docs sujos (sem o index). Disparado pelo debounce per-doc.
    bool flushDirtyDocsNow();

signals:
    void projectSaved();
    void saveFailed(QString error);
    void savingChanged(bool saving);

private slots:
    void onContentFlushed(const QString& key);
    void onAutosaveTimeout();
    void onSettingsChanged();

private:
    bool writeDirtyDocs(QString* errorOut);
    bool writeIndexNow(QString* errorOut);
    void setSaving(bool saving);

    ProjectModel* m_model;
    DocCache* m_cache;
    EditorHost* m_host;
    ElementsStore* m_elementsStore = nullptr;
    WordCounter* m_wordCounter = nullptr;
    QString m_root;
    QTimer* m_perDocTimer;
    QTimer* m_autosaveTimer;
    bool m_saving = false;
    bool m_pendingFullSave = false;
    bool m_settingsDirty = false;
    QString m_lastError;
};
