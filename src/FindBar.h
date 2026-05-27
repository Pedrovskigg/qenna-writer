#pragma once

#include <QList>
#include <QPair>
#include <QTextEdit>
#include <QVector>
#include <QWidget>

class QLabel;
class QLineEdit;
class QToolButton;

// Barra de busca local (Ctrl+F). Flutua sobre o editor, varre o documento ativo
// e mantém uma lista de matches; aplica seleções via signal pra MainWindow
// integrar com outras camadas (focus mode, etc.).
class FindBar : public QWidget {
    Q_OBJECT
public:
    explicit FindBar(QWidget* parent = nullptr);

    void attachTo(QTextEdit* editor);
    void openBar();
    void closeBar();

    void applyTheme();

    // Snapshot da camada "find" pra o sistema de ExtraSelections do MainWindow.
    const QList<QTextEdit::ExtraSelection>& findSelections() const { return m_selections; }

signals:
    void selectionsChanged();
    void closed();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onQueryChanged(const QString& q);
    void onPrev();
    void onNext();
    void onCloseClicked();

private:
    void refreshMatches();
    void rebuildSelections();
    void scrollToMatch(int idx);
    void updateCountLabel();

    QTextEdit* m_editor = nullptr;
    QLineEdit* m_input = nullptr;
    QLabel* m_count = nullptr;
    QToolButton* m_prev = nullptr;
    QToolButton* m_next = nullptr;
    QToolButton* m_close = nullptr;

    // (position, length) absolutos no QTextDocument.
    QVector<QPair<int, int>> m_matches;
    int m_activeIndex = 0;

    QList<QTextEdit::ExtraSelection> m_selections;
};
