#pragma once
// O campo das tags livres (29/10/2026): as tags do item viram pílulas ("guerra ×")
// dentro de uma caixa, com uma linha pra digitar a próxima (Enter, vírgula ou Tab
// confirma; Backspace no vazio tira a última). Embaixo, as tags que o projeto já
// usa, das mais usadas pras menos, pra reaproveitar a mesma grafia; digitar filtra.
// Tudo DENTRO do widget, sem janela suspensa (sai transparente no Windows sobre as
// folhas). Desenhado com a paleta da página (folhas, DocHeader, Criador de Mundos).

#include <QStringList>
#include <QWidget>

#include <functional>

class QLineEdit;
class TagChip;

class TagField : public QWidget {
    Q_OBJECT
public:
    explicit TagField(QWidget* parent = nullptr);

    void setTags(const QStringList& tags);
    // as tags do item, incluindo o que ficou digitado sem confirmar
    QStringList tags() const;
    // as tags do projeto (padrão: ProjectTags::vocabulary() na hora de abrir)
    void setVocabulary(const QStringList& vocab);
    void setMaxSuggestions(int n);
    // sugestões só enquanto se digita (campo fixo na página); nas folhas elas ficam sempre à vista
    void setSuggestOnFocus(bool on);
    void setPlaceholder(const QString& text);
    void focusInput();
    QLineEdit* input() const { return m_input; }

    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    std::function<void()> resized;   // mudou de altura: quem segura (a folha) se ajusta

signals:
    void tagsChanged(const QStringList& tags);   // pôs ou tirou uma tag (não a cada letra)

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    bool eventFilter(QObject* o, QEvent* e) override;

private:
    void commit(const QString& raw);
    void removeTag(const QString& tag);
    void rebuildChips();
    void rebuildSuggestions();
    int arrange(int w, bool apply) const;
    void relayout();
    void styleInput();

    QStringList m_tags, m_vocab;
    QList<TagChip*> m_chips, m_suggest;
    QLineEdit* m_input = nullptr;
    QString m_placeholder, m_inkSeen;
    int m_maxSuggest = 8;
    bool m_suggestOnFocus = false;
    mutable int m_boxH = 34;
    int m_lastH = -1;
};
