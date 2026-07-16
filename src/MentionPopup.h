#pragma once

#include <QObject>
#include <QString>

class ProjectModel;
class ConstrutorStore;
struct Chapter;
class QListWidgetItem;
class QTextEdit;
class QListWidget;
class QWidget;
class QEvent;
class QRegularExpressionMatch;

// Autocomplete de menções (@). Observa um ou mais QTextEdit: quando o usuário
// digita "@", abre uma lista de documentos do projeto; conforme digita, filtra.
// Espaço/Enter/Tab confirmam o item mais compatível, inserindo uma menção como
// link (anchor "ref:<drawerKey>:<itemId>") — o "@" some, fica só o nome.
//
// Suporta drill-down: Root → Capítulos → Cenas, e Root → Sistemas (Construtor)
// → Nós de um sistema (navegação por portal/back).
// Suporta atalho: "@ch1-sc2" insere diretamente o Capítulo 1, Cena 2.
//
// O clique (Ctrl+clique) que abre a menção é tratado no editor (SpellEditor) /
// nos QTextEdit da ficha, não aqui.
class MentionPopup : public QObject {
    Q_OBJECT
public:
    MentionPopup(ProjectModel* model, QWidget* ownerWindow, QObject* parent = nullptr);

    // Passa a observar este editor (instala eventFilter + conexões).
    void attach(QTextEdit* editor);

    // Config: incluir capítulos/cenas na lista e aceitar atalho @ch1-sc2 (padrão: só gavetas).
    void setIncludeManuscripts(bool on) { m_includeManuscripts = on; }

    // Store do Construtor — habilita o portal "Construtor" no root da menção,
    // listando sistemas/nós (regras/seções) pra referenciar via @.
    void setConstrutorStore(ConstrutorStore* store) { m_construtorStore = store; }

signals:
    // Emitido após o vigia limpar um anchor herdado (mexe no documento de forma
    // assíncrona). Quem depende do estado do doc (ex.: Focus Mode) deve reagir.
    void documentTouched();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    enum class Level { Root, Chapters, Scenes, ConstrutorSystems, ConstrutorSystemNodes };

    struct DocEntry {
        QString drawerKey;
        QString itemId;
        QString title;
        QString subtitle;
    };

    void updateForEditor(QTextEdit* ed);
    void onContentsChange(QTextEdit* ed, int pos, int removed, int added);
    void rebuildList(const QString& query);
    void rebuildRoot(const QString& q);
    void rebuildChapters();
    void rebuildScenes();
    void rebuildConstrutorSystems();
    void rebuildConstrutorSystemNodes();
    void buildShorthand(const QRegularExpressionMatch& m);
    void addPortalItem(const QString& text, const QString& key);
    void addBackItem(const QString& text);
    void selectFirstSelectable();
    bool hasSelectableItems() const;
    const Chapter* findDrillChapter() const;
    void drillBack();
    QListWidgetItem* makeDocItem(const DocEntry& d) const;
    void showBelowCursor(QTextEdit* ed);
    void hidePopup();
    void moveSel(int delta);
    void confirm();
    QList<DocEntry> allDocs() const;

    ProjectModel* m_model;
    ConstrutorStore* m_construtorStore = nullptr;
    QWidget*      m_owner;
    QListWidget*  m_list          = nullptr;
    QTextEdit*    m_activeEditor  = nullptr;
    int           m_atPos         = -1;
    QString       m_currentQuery;
    Level         m_level         = Level::Root;
    QString       m_drillManuscriptId;
    QString       m_drillChapterId;
    QString       m_drillChapterTitle;
    QString       m_drillConstrutorSystemId;
    QString       m_drillConstrutorSystemName;
    bool          m_includeManuscripts = false;
    bool          m_insertingMention   = false;
    bool          m_cleaningAnchor     = false;
};
