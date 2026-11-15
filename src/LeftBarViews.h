#pragma once

// Peças dos estilos da LeftBar (14/11/2026, LeftBar Lab aprovado pelo Pe):
// o Tambor de gavetas, a Barra com nomes e os bilhetinhos que saem da barra
// (a lingueta com o nome da gaveta do tambor, o cartão "Nesta cena" e o fio
// que liga a gaveta com cor ao painel). A barra de hoje continua no
// LeftBar.cpp; aqui mora só o que os estilos novos acrescentam.

#include <QHash>
#include <QList>
#include <QPixmap>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QWidget>

class QVariantAnimation;

struct LeftBarDrawerInfo {
    QString key;
    QString title;
    QString color;      // hex da gaveta
    QString iconId;     // :/icons/elements/<id>.svg; vazio = letra
    int count = 0;
    struct Item { QString id; QString title; };
    QList<Item> items;
};

// O ícone da gaveta (SVG tingido ou a inicial) num quadrado de `side` px.
QPixmap leftBarDrawerGlyph(const LeftBarDrawerInfo& d, int side, const QColor& color, qreal dpr);

// ============================================================ Tambor
// As gavetas giram num tambor: a do meio fica grande e acesa, as vizinhas
// curvam pra longe. Roda do mouse, arrasto (encaixa ao soltar) ou clique numa
// vizinha giram; clique na do meio abre a gaveta.
class LeftBarDrum : public QWidget {
    Q_OBJECT
public:
    explicit LeftBarDrum(QWidget* parent = nullptr);

    void setDrawers(const QList<LeftBarDrawerInfo>& drawers);
    void setActiveKey(const QString& key);
    void revealKey(const QString& key);
    QString centerKey() const;
    const LeftBarDrawerInfo* centerInfo() const;
    int centerY() const;
    qreal centerF() const;
    void refreshLook();   // tema/escala mudaram

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void activated(QString key);
    void contextRequested(QString key, QPoint globalPos);
    void centerChanged();
    void moved();   // a cada passo do giro (a lingueta acompanha o miolo)

protected:
    void paintEvent(QPaintEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;
    bool event(QEvent*) override;

private:
    struct Slot { int index; QRectF box; qreal opacity; qreal dist; };
    QList<Slot> layoutSlots() const;
    int indexAt(const QPoint& p) const;
    int slotH() const;
    void setPos(qreal p);
    void animateTo(int index);
    void stopAnim();

    QList<LeftBarDrawerInfo> m_drawers;
    QList<QPixmap> m_glyphs;
    QString m_activeKey;
    qreal m_pos = 0;
    int m_lastCenter = -1;
    QPointer<QVariantAnimation> m_anim;
    qint64 m_lastWheel = 0;
    int m_wheelAcc = 0;
    bool m_pressed = false, m_dragging = false;
    int m_pressY = 0;
    qreal m_pressPos = 0;
    int m_hover = -1;
};

// ============================================================ Barra com nomes
// A barra larga, como a dos apps de hoje: o projeto em cima, as ferramentas
// escritas, as gavetas com quantos itens têm, abrindo em árvore ali mesmo.
// Pintada inteira (com rolagem própria no miolo), sempre na largura de
// projeto; na animação de recolher, a barra corta o que sobra.
class LeftBarNamed : public QWidget {
    Q_OBJECT
public:
    struct Tool { QString id; QString text; QString iconRes; bool active = false; };
    struct Group { QString title; QList<Tool> tools; };

    explicit LeftBarNamed(QWidget* parent = nullptr);

    static int designWidth();

    void setProject(const QString& title, const QString& subtitle, bool infoActive);
    void setGroups(const QList<Group>& groups);
    void setDrawers(const QList<LeftBarDrawerInfo>& drawers, const QString& activeKey);
    void setMirrored(bool on) { m_mirrored = on; update(); }
    void refreshLook();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void infoClicked();
    void toolClicked(QString id);
    void openDrawerRequested(QString key);
    void itemActivated(QString drawerKey, QString itemId);
    void newDrawerClicked();
    void drawerContextRequested(QString key, QPoint globalPos);
    void collapseClicked();
    void contentChanged();
    void emptyContextRequested(QPoint globalPos);   // clique direito fora das gavetas   // uma gaveta abriu/fechou na árvore (a altura mudou)

protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private:
    enum Kind { Head, ToolRow, DrawerRow, ItemRow, NewRow };
    struct Row { Kind kind; int y = 0, h = 0; QString id; QString key; QString text; QString iconRes;
                 QString color; QString count; bool active = false; bool open = false; int drawer = -1; };
    void rebuildRows();
    int headerH() const;
    int footerH() const;
    int bodyTop() const { return headerH(); }
    int bodyH() const { return qMax(0, height() - headerH() - footerH()); }
    int contentH() const;
    void clampScroll();
    int rowAt(const QPoint& p) const;     // -1 nenhum
    bool openHit(const Row& r, const QPoint& p) const;
    QRect footerRect() const;
    QRect headerRect() const;
    QPixmap toolIcon(const QString& res, bool active);
    QPixmap drawerIcon(int drawer);
    void saveOpen();

    QString m_title, m_subtitle;
    bool m_infoActive = false;
    QList<Group> m_groups;
    QList<LeftBarDrawerInfo> m_drawers;
    QString m_activeKey;
    QSet<QString> m_open;
    QList<Row> m_rows;
    int m_scroll = 0;
    int m_hover = -1;            // linha; -2 = cabeçalho; -3 = rodapé
    bool m_hoverOpen = false;    // em cima do botãozinho de abrir a gaveta
    int m_press = -1;
    bool m_mirrored = false;
    QHash<QString, QPixmap> m_iconCache;
};

// ============================================================ Bilhetes
// Lingueta com o nome da gaveta do meio do tambor. Colada na borda da barra,
// não pega o mouse.
class LeftBarTag : public QWidget {
public:
    explicit LeftBarTag(QWidget* parent);
    void setInfo(const QString& title, const QString& color, int count, bool mirrored, bool animate);
    void placeAt(int barEdgeX, int centerY);   // borda da barra, centro na altura
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QString m_title, m_color;
    int m_count = 0;
    bool m_mirrored = false;
    qreal m_t = 1.0;
    QPointer<QVariantAnimation> m_anim;
};

// O cartão "Nesta cena": quem daquela gaveta aparece na folha aberta.
class LeftBarSceneCard : public QWidget {
public:
    explicit LeftBarSceneCard(QWidget* parent);
    void setContent(const QString& drawerTitle, const QString& color, const QString& heading,
                    const QStringList& names);
    void placeAt(int x, int centerY, bool mirrored);   // x = borda de onde ele sai
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QSize contentSize() const;
    QString m_title, m_color, m_heading;
    QStringList m_names;
};

// O fio da gaveta com cor: do ladrilho aceso até a beira do painel aberto.
class LeftBarThread : public QWidget {
public:
    explicit LeftBarThread(QWidget* parent);
    void setColor(const QColor& c) { m_color = c; update(); }
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QColor m_color;
};
