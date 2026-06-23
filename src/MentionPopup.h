#pragma once

#include <QObject>
#include <QString>

class ProjectModel;
class QTextEdit;
class QListWidget;
class QWidget;
class QEvent;

// Autocomplete de menções (@). Observa um ou mais QTextEdit: quando o usuário
// digita "@", abre uma lista de documentos do projeto; conforme digita, filtra.
// Espaço/Enter/Tab confirmam o item mais compatível, inserindo uma menção como
// link (anchor "ref:<drawerKey>:<itemId>") — o "@" some, fica só o nome.
//
// O clique (Ctrl+clique) que abre a menção é tratado no editor (SpellEditor) /
// nos QTextEdit da ficha, não aqui.
class MentionPopup : public QObject {
    Q_OBJECT
public:
    MentionPopup(ProjectModel* model, QWidget* ownerWindow, QObject* parent = nullptr);

    // Passa a observar este editor (instala eventFilter + conexões).
    void attach(QTextEdit* editor);

    // Config: incluir capítulos/manuscritos na lista (padrão: só gavetas).
    void setIncludeManuscripts(bool on) { m_includeManuscripts = on; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct DocEntry {
        QString drawerKey;
        QString itemId;
        QString title;
        QString subtitle;   // nome da gaveta (contexto)
    };

    void updateForEditor(QTextEdit* ed);   // detecta @ e a query
    // Remove o anchor de menção que o texto recém-digitado herda do trecho à
    // esquerda (raiz do "vazamento" do link pro resto da linha/parágrafo).
    void onContentsChange(QTextEdit* ed, int pos, int removed, int added);
    void rebuildList(const QString& query);
    void showBelowCursor(QTextEdit* ed);
    void hidePopup();
    void moveSel(int delta);
    void confirm();
    QList<DocEntry> allDocs() const;

    ProjectModel* m_model;
    QWidget* m_owner;
    QListWidget* m_list = nullptr;
    QTextEdit* m_activeEditor = nullptr;
    int m_atPos = -1;                 // posição absoluta do '@' no documento
    bool m_includeManuscripts = false;
    bool m_insertingMention = false;  // guard: a inserção da própria menção é legítima
    bool m_cleaningAnchor = false;    // guard: evita reentrância ao limpar
};
