#pragma once

#include <QColor>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QTextBlock>
#include <QTextCursor>
#include <QTimer>
#include <functional>

#include "DialogueStore.h"

class DocHeaderBar;
class EditorHost;
class ElementsStore;
class GrannaPickPopup;
class ProjectModel;
class QTextEdit;

// O Granna avisando no DocHeader quem disse a fala que acabou de ser escrita
// (ideia do Pe, 2026-10-02). Regras:
//  - só avisa quando o parágrafo da fala TERMINA (Enter ou cursor saindo dele),
//    nunca enquanto a pessoa ainda digita;
//  - só avisa atribuição NOVA: fala que não existia, ou que mudou de dono ou
//    de certeza. Corrigir um erro de digitação numa fala já atribuída não pisca;
//  - figurante não avisa (tem locutor, só não é do elenco);
//  - clicar no aviso abre "Quem disse?" (GrannaPickPopup), que grava no
//    DialogueStore sem sair do editor.
// O scan em si continua sendo do MainWindow (runDialogueScan): aqui ele só é
// antecipado, pra o aviso não esperar o debounce de 3 s da digitação.
class GrannaNotifier : public QObject {
    Q_OBJECT
public:
    struct Deps {
        QTextEdit* editor = nullptr;
        EditorHost* host = nullptr;
        ProjectModel* model = nullptr;
        ElementsStore* elements = nullptr;
        DialogueStore* dialogues = nullptr;
        DocHeaderBar* header = nullptr;
        std::function<bool()> enabled;  // detecção de diálogos ligada
        std::function<void()> scanNow;  // roda o scan do documento aberto já
    };

    GrannaNotifier(const Deps& deps, QObject* parent = nullptr);

    // Documento novo no editor: esquece o parágrafo e apaga o aviso.
    void reset();

private:
    struct Before { QString id; QString characterId; QString confidence; };

    void onCursorMoved();
    void trackBlock(const QTextBlock& block);
    void announce();
    void openPicker();
    // Cursor parado numa fala que já existia: a foto dela fica no cabeçalho
    // enquanto ele estiver no parágrafo (ideia do Pe, 2026-10-02).
    void showCurrent();
    void releaseSticky();
    void showMark(const DialogueStore::Dialogue& dl, bool sticky);
    const DialogueStore::Dialogue* currentDialogue(const QString& chapterId) const;
    const DialogueStore::Dialogue* dialogueById(const QString& id) const;
    bool activeDoc(QString* chapterId = nullptr) const;
    QColor voiceOf(const QString& characterId, const QString& manuscriptId) const;

    Deps d;
    // Cursor de apoio no começo do parágrafo atual: o documento ajusta a
    // posição dele a cada edição, então dá pra achar o parágrafo de novo na
    // saída sem guardar um QTextBlock que pode ter morrido.
    QTextCursor m_track;
    bool m_tracking = false;
    QString m_enterText;               // texto do parágrafo quando o cursor entrou
    QHash<QString, Before> m_enterState; // falas daquele parágrafo, como estavam na entrada

    // Parágrafo que acabou de terminar, esperando o scan.
    QString m_pendingChapter;
    QStringList m_pendingTexts;
    QHash<QString, Before> m_pendingBefore;
    QTimer m_timer;

    QString m_markId;     // fala do aviso na tela
    QString m_stickyKey;  // id|dono|certeza da foto fixa na tela ("" = nenhuma)
    bool m_announcing = false; // aviso que some sozinho em andamento
    QPointer<GrannaPickPopup> m_popup;
};
