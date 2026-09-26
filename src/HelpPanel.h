#pragma once

#include <QFrame>
#include <QString>
#include <QStringList>
#include <QVector>

class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTextBrowser;
class QToolButton;
class QUrl;
class QVBoxLayout;

// Janela de Ajuda (não modal, sem auto-fechar) — fica aberta lado a lado com
// o app enquanto o usuário segue as instruções. Estilo "Manual": trilho com
// os grupos de tópicos (e a busca), a lista do grupo ao lado, o texto numa
// coluna de leitura (QTextBrowser, com <img> clicáveis que abrem ampliadas) e,
// na margem direita, o índice dos passos do tópico ("Neste tópico").
class HelpPanel : public QFrame
{
    Q_OBJECT
public:
    explicit HelpPanel(QWidget* parent = nullptr);

    // Só posiciona a janela da primeira vez que abre; chamadas seguintes só
    // trazem pra frente, preservando onde/como o usuário deixou a janela.
    void openNear(const QRect& anchorGlobal, Qt::Edge barSide = Qt::TopEdge);

private slots:
    void onTopicSelected();
    void onAnchorClicked(const QUrl& url);
    void applyTheme();

private:
    struct Topic {
        QString id;
        QString label;
    };
    struct Group {
        QString label;
        QString icon;
        QStringList topicIds;
    };
    // Um pedaço de tópico pra busca: a introdução (step -1) ou um passo.
    struct SearchChunk {
        QString topicId;
        int step = -1;
        QString stepTitle;
        QString folded;
    };

    void buildUi();
    void buildTopics();
    void rebuildList();
    void updateContent();
    void selectGroup(int group);
    void selectTopic(const QString& id, int step = -1);
    void setSearchMode(bool on);
    void runSearch();
    void rebuildToc(const QStringList& steps);
    void updateTocHighlight();
    void refreshRailIcons();
    int groupOf(const QString& id) const;
    QString labelOf(const QString& id) const;
    QString decorate(const QString& id, const QString& html, QStringList* steps) const;
    bool isNew(const QString& id) const;
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
    QString createDocumentsContent() const;
    QString themesContent() const;
    QString builderContent() const;
    QString pensarioContent() const;
    QString bondsContent() const;
    QString worldMapContent() const;
    QString glossaryContent() const;
    QString statsContent() const;
    QString ambienceContent() const;
    QString remindersContent() const;
    QString sceneVariationContent() const;

    QVector<Topic> m_topics;
    QVector<Group> m_groups;
    QVector<SearchChunk> m_searchIndex;
    QList<QToolButton*> m_railButtons;
    QToolButton* m_searchButton = nullptr;
    QLabel* m_listTitle = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QListWidget* m_list = nullptr;
    QTextBrowser* m_content = nullptr;
    QWidget* m_tocPanel = nullptr;
    QVBoxLayout* m_tocLayout = nullptr;
    QList<QPushButton*> m_tocButtons;
    int m_group = 0;
    bool m_searchMode = false;
    bool m_filling = false;

    QString m_selectedId;
    bool m_positioned = false;
};
