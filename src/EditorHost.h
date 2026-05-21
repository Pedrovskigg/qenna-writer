#pragma once

#include <QObject>
#include <QString>

class QTextEdit;
class QTimer;
class DocCache;
class ProjectModel;

class EditorHost : public QObject {
    Q_OBJECT
public:
    enum ViewModeType {
        Disabled,
        ChapterDoc,
        SceneDoc,
        DrawerDoc,
        ManuscriptDoc,
    };

    struct ViewMode {
        ViewModeType type = Disabled;
        QString manuscriptId;
        QString chapterId;
        int sceneIndex = -1;
        QString itemId;

        bool operator==(const ViewMode& other) const;
        bool operator!=(const ViewMode& other) const { return !(*this == other); }
    };

    EditorHost(QTextEdit* editor, DocCache* cache, ProjectModel* model, QObject* parent = nullptr);

    QTextEdit* editor() const { return m_editor; }
    ViewMode viewMode() const { return m_viewMode; }

    void setProjectRoot(const QString& root);
    QString projectRoot() const { return m_projectRoot; }
    QString activeKey() const;
    static QString keyFor(const ViewMode& vm);

    void setViewMode(const ViewMode& vm);
    void disable() { setViewMode(ViewMode{}); }

    void syncEditorToCache();

    void setFlushDebounceMs(int ms);

    // Variação da cena ativa. Operam quando viewMode é SceneDoc.
    QString createVariationForCurrentScene(const QString& label = QString());
    bool switchVariationForCurrentScene(const QString& variationId);
    bool deleteVariationForCurrentScene(const QString& variationId);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void viewModeChanged();
    void contentFlushed(QString key);
    void contentLoaded();

private slots:
    void onEditorTextChanged();
    void flushPendingEditorContent();

private:
    void loadIntoEditor(const QString& html);
    void replaceContent(const QString& html);
    QString hydrateFromModel(const ViewMode& vm) const;
    bool tryConvertHrShortcut();

    QTextEdit* m_editor;
    DocCache* m_cache;
    ProjectModel* m_model;
    ViewMode m_viewMode;
    QTimer* m_flushTimer;
    QString m_projectRoot;
    bool m_loadingContent = false;
};
