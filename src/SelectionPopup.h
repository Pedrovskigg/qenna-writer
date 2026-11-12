#ifndef SELECTIONPOPUP_H
#define SELECTIONPOPUP_H

#include <QFrame>
#include <QHash>
#include <QPixmap>
#include <QString>
#include <QVector>
#include <functional>

class QFrame;
class QLabel;
class QPainterPath;
class QTextEdit;
class QToolButton;
class QHBoxLayout;
class SelectionCarousel;
class SelectionGroupDots;

// Mini-toolbar flutuante que aparece acima de uma seleção de texto, estilo
// Medium/Notion. Sem foco próprio (não rouba o cursor do editor). Ações são
// adicionadas via addAction; o popup é a infraestrutura compartilhada para
// CreateDocFrom, Marcadores, Glossário e formatação inline.
//
// Desenho "carrossel" (12/11/2026, ideia do Pe): o balão tem largura curta e
// a fita de botões gira pra mostrar o resto, com as pontas curvando como um
// tambor. Normal: a fita acompanha o mouse. O ↔ na lingueta de baixo liga o
// modo "só pelas pontas" (o meio fica parado pra mirar); a escolha é salva.
class SelectionPopup : public QFrame
{
    Q_OBJECT

public:
    explicit SelectionPopup(QTextEdit *editor, QWidget *parent = nullptr);

    using Callback = std::function<void()>;

    QToolButton* addAction(const QString &iconAlias, const QString &tooltip, Callback cb);
    void addSeparator();
    // Abre um grupo: os próximos addAction entram numa ilha com o título
    // pequeno embaixo (Formato, Anotar, Criar…). Entre grupos, um fio.
    void beginGroup(const QString &title);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void onSelectionChanged();
    void onScrollChanged();
    void applyTheme();

private:
    void updatePosition();
    bool hasSelection() const;
    void applyRootStyle();
    void reloadAllIcons();
    void refitStrip();
    void setEdgeMode(bool on);
    QPainterPath balloonPath() const;

    QTextEdit *m_editor;
    QWidget *m_strip = nullptr;          // fita com todos os botões (desenhada pelo carrossel)
    QHBoxLayout *m_layout;               // layout da fita
    SelectionCarousel *m_carousel = nullptr;
    SelectionGroupDots *m_dots = nullptr;
    QToolButton *m_modeBtn = nullptr;    // o ↔ da lingueta
    bool m_dragging = false;

    // Pra recolorir ícones em themeChanged sem rebuildar os botões.
    QHash<QToolButton*, QString> m_iconAliasByBtn;
    QVector<QFrame*> m_separators;
    QHBoxLayout *m_groupRow = nullptr;
    QVector<QLabel*> m_groupTitles;
    QVector<QWidget*> m_groupBoxes;
    // Sombra + balão desenhados uma vez por tamanho/tema (a fita repinta a 60 fps
    // e, com a janela transparente, o fundo é redesenhado junto).
    QPixmap m_bgCache;
    void styleGroupTitle(QLabel *l);
};

#endif
