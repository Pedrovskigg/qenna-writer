#ifndef SELECTIONPOPUP_H
#define SELECTIONPOPUP_H

#include <QFrame>
#include <functional>

class QTextEdit;
class QToolButton;
class QHBoxLayout;

// Mini-toolbar flutuante que aparece acima de uma seleção de texto, estilo
// Medium/Notion. Sem foco próprio (não rouba o cursor do editor). Ações são
// adicionadas via addAction; o popup é a infraestrutura compartilhada para
// CreateDocFrom, Marcadores, Glossário e formatação inline.
class SelectionPopup : public QFrame
{
    Q_OBJECT

public:
    explicit SelectionPopup(QTextEdit *editor, QWidget *parent = nullptr);

    using Callback = std::function<void()>;

    QToolButton* addAction(const QString &iconAlias, const QString &tooltip, Callback cb);
    void addSeparator();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onSelectionChanged();
    void onScrollChanged();
    void applyTheme();

private:
    void updatePosition();
    bool hasSelection() const;
    void applyRootStyle();

    QTextEdit *m_editor;
    QHBoxLayout *m_layout;
    bool m_dragging = false;
};

#endif
