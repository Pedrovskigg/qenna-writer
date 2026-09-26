#pragma once
// "Termo no texto": os termos do glossário ganham um sublinhado pontilhado
// discreto no editor e, parando o mouse em cima, uma ficha com o tipo, a
// definição e a primeira vez que o termo aparece no livro. Liga e desliga nas
// Configurações (QSettings "editor/glossaryInText").
//
// O sublinhado é uma camada de ExtraSelections — quem aplica é o MainWindow
// (setEditorSelectionsLayer), que junta as camadas de todo mundo.

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTextEdit>
#include <QVector>
#include <functional>

class QFrame;
class QLabel;
class QTimer;
class QToolButton;
class GlossaryIndex;
class GlossaryStore;

class GlossaryInText : public QObject {
    Q_OBJECT
public:
    using ApplyLayer = std::function<void(const QList<QTextEdit::ExtraSelection>&)>;
    GlossaryInText(QTextEdit* editor, GlossaryStore* store, GlossaryIndex* index,
                   ApplyLayer apply, QObject* parent = nullptr);

    static bool enabledSetting();
    void setEnabled(bool on);
    bool isEnabled() const { return m_enabled; }
    // Recalcula o sublinhado (troca de documento, tema, glossário mudou).
    void scheduleRefresh();

signals:
    void openInGlossaryRequested(QString entryId);
    void goToFirstUseRequested(QString docKey, QString sentence);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Hit { int start = 0; int end = 0; QString entryId; };
    void refresh();
    void hoverAt(const QPoint& viewportPos);
    void showCard(const QString& entryId, const QPoint& globalPos);
    void hideCardSoon();
    void ensureCard();
    void applyCardTheme();

    QPointer<QTextEdit> m_editor;
    GlossaryStore* m_store = nullptr;
    GlossaryIndex* m_index = nullptr;
    ApplyLayer m_apply;
    bool m_enabled = true;
    QVector<Hit> m_hits;
    QTimer* m_refreshTimer = nullptr;
    QTimer* m_showTimer = nullptr;
    QTimer* m_hideTimer = nullptr;
    QString m_pendingId, m_cardId;
    QPoint m_pendingPos;

    QFrame* m_card = nullptr;
    QLabel* m_cardTitle = nullptr;
    QLabel* m_cardDef = nullptr;
    QLabel* m_cardMeta = nullptr;
    QToolButton* m_cardOpen = nullptr;
    QToolButton* m_cardGo = nullptr;
    QString m_cardDocKey, m_cardSentence;
};
