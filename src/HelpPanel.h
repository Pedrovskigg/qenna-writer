#pragma once

#include <QFrame>
#include <QString>
#include <QVector>

class QLabel;
class QListWidget;
class QListWidgetItem;
class QTextBrowser;
class QUrl;

// Janela de Ajuda (não modal, sem auto-fechar) — fica aberta lado a lado com
// o app enquanto o usuário segue as instruções. Lista de tópicos à esquerda
// (fixa, não editável pelo usuário), conteúdo em texto rico à direita
// (QTextBrowser, aceita HTML com <img>; imagens inline são clicáveis e abrem
// ampliadas numa janela própria).
class HelpPanel : public QFrame
{
    Q_OBJECT
public:
    explicit HelpPanel(QWidget* parent = nullptr);

    // Só posiciona a janela da primeira vez que abre; chamadas seguintes só
    // trazem pra frente, preservando onde/como o usuário deixou a janela.
    void openNear(const QRect& anchorGlobal);

private slots:
    void onTopicSelected();
    void onAnchorClicked(const QUrl& url);
    void applyTheme();

private:
    struct Topic {
        QString id;
        QString label;
    };

    void buildUi();
    void buildTopics();
    void rebuildList();
    void updateContent();
    void openImageZoom(const QString& resourcePath);
    QString contentFor(const QString& id) const;
    QString startHereContent() const;
    QString manuscriptsContent() const;
    QString drawersContent() const;
    QString exportContent() const;
    QString editorContent() const;
    QString dailyGoalCounterContent() const;
    QString referenceMenuContent() const;
    QString keyboardShortcutsContent() const;
    QString markersContent() const;
    QString memoriesContent() const;
    QString timelineContent() const;
    QString coverCreatorContent() const;

    QVector<Topic> m_topics;
    QLabel* m_sidebarTitle = nullptr;
    QListWidget* m_list = nullptr;
    QLabel* m_contentTitle = nullptr;
    QTextBrowser* m_content = nullptr;

    QString m_selectedId;
    bool m_positioned = false;
};
