#pragma once

#include "AIClient.h"

#include <QFrame>
#include <QJsonObject>
#include <QPoint>
#include <QString>
#include <QVector>
#include <functional>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTextEdit;
class QToolButton;

// Janela de chat compacta e arrastável (mesmo padrão do BondPopup: QFrame
// filho de editorContainer, sem moldura de janela nativa, posicionada via
// move()/raise()), aberta a partir do botão "Revisar com a assistente" do
// SelectionPopup.
//
// O trecho selecionado fica anexado/fixo no topo — a conversa continua
// mesmo que a seleção no editor mude depois de aberta. Chips de ação rápida
// disparam pedidos prontos; o campo de texto livre permite um pedido
// específico. O checkbox de contexto amplo é controlado manualmente pelo
// usuário (nunca pela IA sozinha) para manter o custo de API previsível —
// ver decisão no item 9 do plano (memória ai-assistant-feature).
//
// Quando a IA propõe um texto de substituição concreto, ela usa uma tool
// real (propose_edit), não parsing de regex em cima da resposta livre — o
// resultado vira um cartão de sugestão com botão de aplicar, que substitui
// exatamente o range da seleção original guardado na abertura do chat
// (mesmo que a seleção no editor tenha mudado nesse meio-tempo).
class AISelectionChat : public QFrame {
    Q_OBJECT
public:
    // editor/selectionStart/selectionEnd: onde aplicar a sugestão quando o
    // usuário clicar em "Aplicar". sceneTextProvider: retorna o texto do
    // documento atualmente aberto (cena ou capítulo) para anexar quando o
    // usuário marcar o checkbox de contexto amplo. Se for nullptr, o
    // checkbox não aparece.
    AISelectionChat(QWidget* parent,
                     QTextEdit* editor,
                     int selectionStart,
                     int selectionEnd,
                     const QString& selectedText,
                     const QString& charactersContext,
                     std::function<QString()> sceneTextProvider);

signals:
    void closeRequested();

protected:
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;

private:
    void buildUi();
    QString buildSystemPrompt() const;
    void sendUserMessage(const QString& displayText, const QString& payloadText);
    void appendTranscriptEntry(const QString& speakerLabel, const QString& text, bool isAssistant);
    void beginAssistantStreamEntry();
    void appendStreamToken(const QString& token);
    void setBusy(bool busy);
    void onToolCallReceived(const QString& id, const QString& name, const QJsonObject& arguments);
    void showSuggestion(const QString& text);
    void applySuggestion();
    void dismissSuggestion();

    AIClient* m_client;
    QTextEdit* m_editor;
    int m_selectionStart;
    int m_selectionEnd;
    QString m_selectedText;
    QString m_charactersContext;
    std::function<QString()> m_sceneTextProvider;
    QVector<AIChatMessage> m_messages;
    bool m_assistantTurnOpen = false;
    QString m_pendingSuggestion;

    QWidget* m_header = nullptr;
    QToolButton* m_closeBtn = nullptr;
    QTextEdit* m_transcript = nullptr;
    QLineEdit* m_inputEdit = nullptr;
    QPushButton* m_sendBtn = nullptr;
    QCheckBox* m_fullContextCheck = nullptr;
    QLabel* m_statusLabel = nullptr;

    QWidget* m_suggestionPanel = nullptr;
    QTextEdit* m_suggestionText = nullptr;
    QPushButton* m_applyBtn = nullptr;
    QPushButton* m_dismissBtn = nullptr;

    bool m_dragging = false;
    QPoint m_dragOffset;
};
