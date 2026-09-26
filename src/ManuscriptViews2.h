#pragma once
// Peças desenhadas dos estilos novos da gaveta de Manuscritos (levas 6 a 8):
// o topo da Seleção de capítulo (arte + checkpoints), o Tambor, a página do
// Álbum, o Carrossel de capas, as lombadas do Encadernado e o Leque. Como em
// ManuscriptViews.h, nenhuma sabe de ProjectModel: o ManuscriptPanel entrega
// os dados prontos e escuta os sinais.

#include <QColor>
#include <QList>
#include <QPixmap>
#include <QPointer>
#include <QString>
#include <QWidget>

class QTimer;
class QVariantAnimation;

namespace MsFonts {
// Primeira família instalada da lista (as fontes vêm embarcadas no app, mas o
// nome registrado varia entre a versão estática e a variável).
QString pick(const QStringList& families);
QString display();    // condensada de título de jogo (Big Shoulders)
QString garamond();   // EB Garamond
QString bodoni();     // Bodoni Moda
QString cormorant();  // Cormorant Garamond
QString sticker();    // Archivo Black
}

// ---- Seleção de capítulo: a arte do capítulo ocupando o topo ----
class MsHero : public QWidget {
    Q_OBJECT
public:
    struct Data {
        QPixmap art;          // vinheta larga do capítulo (já no tamanho)
        QColor tint;          // cor da Parte/livro, pro rótulo de cima
        QString eyebrow;      // "PARTE I · O ARQUIVO"
        QString big;          // "CAPÍTULO 3"
        QString title;        // "The Silk Room"
        QString meta;         // "3 cenas · 3.415 palavras · Isabel"
        QString bookChip;     // "LIVRO 2 ⇄"
        bool preview = false; // o mouse está em outro capítulo
        QString previewText;  // "PRÉVIA"
    };
    explicit MsHero(QWidget* parent = nullptr);
    void setData(const Data& d);
    QSize sizeHint() const override { return QSize(300, 244); }

signals:
    void bookChipClicked(QPoint globalPos);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;

private:
    QRect chipRect() const;
    Data m_d;
    QPixmap m_prevArt;        // arte anterior, pro cruzamento
    qreal m_fade = 1.0;
    QPointer<QVariantAnimation> m_anim;
};

// Cenas como checkpoints: as que já passaram cheias, a atual brilhando.
class MsCheckpoints : public QWidget {
    Q_OBJECT
public:
    explicit MsCheckpoints(QWidget* parent = nullptr);
    void setData(int count, int here, const QColor& color, const QStringList& labels, const QStringList& tips);
    QSize sizeHint() const override { return QSize(240, 40); }

signals:
    void sceneClicked(int index);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    bool event(QEvent* e) override;

private:
    int hitIndex(const QPoint& p) const;
    int m_count = 0, m_here = -1, m_hover = -1;
    QColor m_color;
    QStringList m_labels, m_tips;
};

// ---- Tambor: seletor giratório ----
class MsDrum : public QWidget {
    Q_OBJECT
public:
    struct Item { QString label; QString title; QString number; bool empty = false; };
    explicit MsDrum(QWidget* parent = nullptr);
    void setItems(const QList<Item>& items, int current, const QColor& accent);
    int current() const { return m_cur; }
    QSize sizeHint() const override { return QSize(260, 300); }

signals:
    void currentChanged(int index);   // o tambor parou num item
    void activated(int index);        // clique no do meio ou Enter

protected:
    void paintEvent(QPaintEvent*) override;
    void wheelEvent(QWheelEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    void spinTo(int index);
    int itemAt(const QPoint& p) const;
    QList<Item> m_items;
    int m_cur = 0;
    qreal m_pos = 0;          // posição animada (índice fracionário)
    QColor m_accent;
    QPointer<QVariantAnimation> m_anim;
    int m_wheelAcc = 0;
    int m_dragY = -1;
    bool m_dragged = false;
    QString m_typed;          // número digitado com o tambor em foco
    QTimer* m_typeTimer = nullptr;
};

// ---- Álbum de figurinhas: uma página (Parte) ----
class MsAlbumPage : public QWidget {
    Q_OBJECT
public:
    struct Sticker {
        QString chapterId;
        QString number;       // "3", "★" (especiais)
        QString title;
        QString meta;         // "3 cenas · 3,4k"
        QPixmap art;          // quadrada
        bool foil = false;    // prólogo, interlúdio, epílogo
        bool empty = false;   // capítulo sem texto: o espaço ainda sem figurinha
        bool current = false;
    };
    explicit MsAlbumPage(QWidget* parent = nullptr);
    void setPage(const QString& eyebrow, const QString& title, const QString& watermark,
                 const QColor& color, const QList<Sticker>& stickers, const QString& countText);
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override;
    QSize sizeHint() const override { return QSize(300, heightForWidth(300)); }
    static int artSide(int width);

signals:
    void stickerClicked(QString chapterId);
    void stickerContextRequested(QString chapterId, QPoint globalPos);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void leaveEvent(QEvent*) override;
    void contextMenuEvent(QContextMenuEvent* e) override;
    void showEvent(QShowEvent*) override;
    void hideEvent(QHideEvent*) override;

private:
    QRect stickerRect(int i) const;
    int hitIndex(const QPoint& p) const;
    QString m_eyebrow, m_title, m_watermark, m_count;
    QColor m_color;
    QList<Sticker> m_stickers;
    int m_hover = -1;
    QTimer* m_foilTimer = nullptr;
    qreal m_foil = 0;
};

// Lista de conferência do álbum: um quadradinho por capítulo.
class MsChecklist : public QWidget {
public:
    explicit MsChecklist(QWidget* parent = nullptr);
    void setData(const QList<bool>& glued, const QStringList& tips);
    QSize sizeHint() const override;
protected:
    void paintEvent(QPaintEvent*) override;
    bool event(QEvent* e) override;
private:
    QList<bool> m_glued;
    QStringList m_tips;
};

// ---- Carrossel de capas (a saga) ----
struct MsCoverItem {
    QString id;
    QString title;
    QPixmap cover;            // já no tamanho do carrossel
};

class MsCarousel : public QWidget {
    Q_OBJECT
public:
    explicit MsCarousel(QWidget* parent = nullptr);
    // fromIndex: onde o carrossel estava (anima até current); -1 = sem animação.
    void setItems(const QList<MsCoverItem>& items, int current, int fromIndex,
                  const QString& title, const QString& meta, const QColor& accent);
    QSize sizeHint() const override { return QSize(280, 262); }

signals:
    void bookClicked(QString id);
    void bookContextRequested(QString id, QPoint globalPos);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;

private:
    QRectF coverRect(int i, qreal& scale, qreal& opacity) const;
    int hitIndex(const QPoint& p) const;
    QList<MsCoverItem> m_items;
    int m_cur = 0;
    qreal m_pos = 0;
    QString m_title, m_meta;
    QColor m_accent;
    QPointer<QVariantAnimation> m_anim;
};

// ---- Encadernado: as lombadas em pé na borda da gaveta ----
class MsSpineStack : public QWidget {
    Q_OBJECT
public:
    struct Spine { QString id; QString title; int number = 0; QColor color; };
    explicit MsSpineStack(QWidget* parent = nullptr);
    void setSpines(const QList<Spine>& spines, const QString& currentId, const QString& addTip);
    int preferredWidth() const;
    QSize sizeHint() const override { return QSize(preferredWidth(), 300); }

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
    int spineWidth() const;
    QRect spineRect(int i) const;     // i == spines.size() → o "+"
    int hitIndex(const QPoint& p) const;
    QList<Spine> m_spines;
    QString m_current, m_addTip;
    int m_hover = -1;
};

// ---- Leque: as capas da saga como cartas na mão ----
class MsFan : public QWidget {
    Q_OBJECT
public:
    explicit MsFan(QWidget* parent = nullptr);
    void setItems(const QList<MsCoverItem>& items, const QString& currentId, const QColor& tint,
                  const QString& label, const QString& addTip);
    QSize sizeHint() const override { return QSize(280, 176); }

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
    QTransform cardTransform(int slot, qreal lift) const;
    int hitIndex(const QPoint& p) const;    // slot; items.size() = "+"
    void setHover(int slot);
    QList<MsCoverItem> m_items;
    QString m_current, m_label, m_addTip;
    QColor m_tint;
    int m_hover = -1;
    QList<qreal> m_lift;       // levantada animada de cada carta
    QPointer<QVariantAnimation> m_anim;
};
