#pragma once
// Peças desenhadas à mão da gaveta de Manuscritos (estilos e ferramentas):
// linha pintada (Sumário, Espinha, Grade de cenas), prateleira (Lombadas e
// Vitrine), Mosaico, Jornada, gráfico de Ritmo, cruzamento Leitura × História
// e a faixa de POV. Nenhuma sabe de ProjectModel: o ManuscriptPanel monta os
// dados e escuta os sinais.

#include <QColor>
#include <QHash>
#include <QList>
#include <QPixmap>
#include <QString>
#include <QToolButton>
#include <QWidget>
#include <functional>

// Linha da lista desenhada pelo próprio painel. É um QToolButton de propósito:
// clique, menu de contexto e o arraste (que o ManuscriptPanel pega por
// eventFilter em QToolButton com a propriedade "kind") continuam valendo.
class MsRow : public QToolButton {
    Q_OBJECT
public:
    using Painter = std::function<void(QPainter&, const QRect&, const MsRow*)>;
    using HitTest = std::function<int(const QPoint&)>;

    explicit MsRow(QWidget* parent = nullptr);
    void setPainter(Painter p) { m_painter = std::move(p); update(); }
    // Pedaços clicáveis dentro da linha (células da Grade). -1 = a linha.
    void setHitTest(HitTest h) { m_hit = std::move(h); setMouseTracking(true); }
    int hoverPart() const { return m_hoverPart; }
    bool isHovered() const { return m_hovered; }
    void setRowHeight(int h) { setFixedHeight(h); }

signals:
    void partClicked(int part);
    void partHovered(int part);

protected:
    void paintEvent(QPaintEvent*) override;
    void enterEvent(QEnterEvent* e) override;
    void leaveEvent(QEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;

private:
    Painter m_painter;
    HitTest m_hit;
    int m_hoverPart = -1;
    bool m_hovered = false;
};

// ---- Prateleira: Lombadas (de lado) e Vitrine (capa de frente) ----
struct MsBook {
    QString id;
    QString title;
    int number = 0;       // posição na saga (1…)
    int words = 0;
    QColor color;         // cor do livro (matiz estável pelo id)
    QPixmap cover;        // capa do usuário; nula = capa gerada
};

class MsShelf : public QWidget {
    Q_OBJECT
public:
    enum Mode { Spines, Covers };
    explicit MsShelf(Mode mode, QWidget* parent = nullptr);
    void setBooks(const QList<MsBook>& books, const QString& currentId);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }
    QString addLabel;     // tooltip do "+"

signals:
    void bookClicked(QString id);
    void addClicked();
    void bookContextRequested(QString id, QPoint globalPos);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void leaveEvent(QEvent*) override;
    void contextMenuEvent(QContextMenuEvent* e) override;
    bool event(QEvent* e) override;

private:
    QRect itemRect(int i) const;   // i == books.size() → o "+"
    int hitIndex(const QPoint& p) const;
    int itemWidth(int i) const;
    Mode m_mode;
    QList<MsBook> m_books;
    QString m_current;
    int m_hover = -1;
    int m_maxWords = 1;
};

// ---- Itens pintados de capítulo (Mosaico, Jornada, Ritmo) ----
struct MsChapterDot {
    QString chapterId;
    QString shortLabel;   // "3", "P", "I"
    QString title;        // título sem número
    QString meta;         // "2.980 · Revisado"
    int words = 0;
    double dialogue = 0.0;
    QColor color;         // cor do status (ou neutra)
    bool special = false; // prólogo/interlúdio/epílogo
    bool current = false;
    bool dim = false;
};

struct MsBand {           // parte, na Jornada
    QString title;
    QColor color;
    int first = 0, last = 0;   // índices em dots
};

class MsMosaic : public QWidget {
    Q_OBJECT
public:
    explicit MsMosaic(QWidget* parent = nullptr);
    void setDots(const QList<MsChapterDot>& dots);
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override;
    QSize sizeHint() const override { return QSize(240, heightForWidth(240)); }

signals:
    void chapterClicked(QString chapterId);
    void chapterContextRequested(QString chapterId, QPoint globalPos);
    void chapterHovered(QString chapterId, QRect globalRect);   // "" = saiu

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void leaveEvent(QEvent*) override;
    void contextMenuEvent(QContextMenuEvent* e) override;

private:
    int cols(int w) const;
    QRect tileRect(int i) const;
    int hitIndex(const QPoint& p) const;
    QList<MsChapterDot> m_dots;
    int m_hover = -1;
};

class MsJourney : public QWidget {
    Q_OBJECT
public:
    explicit MsJourney(QWidget* parent = nullptr);
    void setData(const QList<MsChapterDot>& dots, const QList<MsBand>& bands);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return QSize(200, sizeHint().height()); }

signals:
    void chapterClicked(QString chapterId);
    void chapterContextRequested(QString chapterId, QPoint globalPos);
    void chapterHovered(QString chapterId, QRect globalRect);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void leaveEvent(QEvent*) override;
    void contextMenuEvent(QContextMenuEvent* e) override;

private:
    QPointF nodePos(int i) const;
    int hitIndex(const QPoint& p) const;
    QList<MsChapterDot> m_dots;
    QList<MsBand> m_bands;
    int m_hover = -1;
};

class MsRhythm : public QWidget {
    Q_OBJECT
public:
    explicit MsRhythm(QWidget* parent = nullptr);
    void setDots(const QList<MsChapterDot>& dots);
    QSize sizeHint() const override { return QSize(240, 96); }
    int hoverIndex() const { return m_hover; }

signals:
    void chapterClicked(QString chapterId);
    void hoverChanged(int index);   // -1 = saiu

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void leaveEvent(QEvent*) override;

private:
    QRectF barRect(int i) const;
    int hitIndex(const QPoint& p) const;
    QList<MsChapterDot> m_dots;
    int m_hover = -1;
};

// Leitura (em cima) × História (embaixo): uma linha por capítulo. Os que estão
// fora da maior subsequência crescente (os flashbacks de verdade) acendem.
class MsCrossing : public QWidget {
public:
    explicit MsCrossing(QWidget* parent = nullptr);
    // storyPos[i] = posição na história do capítulo i da leitura.
    void setData(const QList<int>& storyPos, const QList<bool>& jump,
                 const QString& readLabel, const QString& storyLabel);
    QSize sizeHint() const override { return QSize(240, 78); }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QList<int> m_story;
    QList<bool> m_jump;
    QString m_readLabel, m_storyLabel;
};

// Faixa fina com um pedaço por capítulo, na cor do narrador.
class MsPovStrip : public QWidget {
    Q_OBJECT
public:
    struct Seg { QString chapterId; QColor color; bool dim = false; bool current = false; QString tip; };
    explicit MsPovStrip(QWidget* parent = nullptr);
    void setSegments(const QList<Seg>& segs);
    QSize sizeHint() const override { return QSize(240, 14); }

signals:
    void chapterClicked(QString chapterId);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    bool event(QEvent* e) override;

private:
    int hitIndex(const QPoint& p) const;
    QList<Seg> m_segs;
};

namespace MsPaint {
// Rosto redondo do narrador: foto do Element ou a inicial na cor dele.
QPixmap avatar(const QPixmap& photo, const QString& name, const QColor& color, int size, qreal dpr);
// Capa gerada (livro sem capa): cor do livro, título e "Livro N".
QPixmap generatedCover(const QString& title, int number, const QColor& color, QSize size, qreal dpr,
                       const QString& bookWord);
QColor bookColor(const QString& id);
// Cor escolhida pelo usuário por livro (id → cor). Vazio = cor automática.
void setBookColorOverrides(const QHash<QString, QColor>& colors);
QString fmtInt(int n);
}
