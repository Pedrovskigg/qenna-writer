#pragma once
// Estilos desenhados da gaveta (DrawerListPanel): Retratos, Polaroid, Crachás e Galeria.
// Recebem os dados prontos (o painel sabe de ProjectModel/ElementsStore) e
// devolvem sinais: abrir item, menu, hover, criar/abrir vínculo.

#include <QColor>
#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QPixmap>
#include <QPointF>
#include <QString>
#include <QWidget>

struct DwEntry {
    QString id;
    QString title;
    QString role;          // já traduzido, em versalete na tela
    QString oneLine;       // uma frase da ficha
    QPixmap photo;
    QColor roleColor;      // cor do papel (protagonista/antagonista/outros)
    QList<QPair<QString, QString>> facts;  // campos curtos da ficha (Crachás): rótulo, valor
    QList<bool> appears;   // presença por capítulo (ferramenta Aparições); vazio = sem dado
    QString appearsMeta;   // "em 6 de 9 · desde o 1 · última no 7"
};

struct DwBond {
    QString id;
    QString from;
    QString to;
    QColor color;
    QString label;         // tipo do vínculo
};

struct DwSection {
    QString title;
    QList<int> rows;       // índices em entries
};

// Base comum: arrastar de um item até outro cria vínculo (linha elástica).
class DwBondDragBase : public QWidget {
    Q_OBJECT
public:
    explicit DwBondDragBase(QWidget* parent = nullptr);
    bool bondsEnabled = false;   // só gaveta de personagem

    // Cascata de entrada (gaveta abrindo, troca de estilo): cada item entra
    // deslizando da barra, um depois do outro.
    void startIntro(int delayMs);

signals:
    void itemActivated(QString id);
    void itemContextRequested(QString id, QPoint globalPos);
    void itemHovered(QString id, QRect globalRect);   // "" = saiu
    void bondCreateRequested(QString fromId, QString toId, QPoint globalPos);
    void bondClicked(QString bondId, QPoint globalPos);

protected:
    // Subclasses dizem quem está sob o ponto e de onde sai o arraste.
    virtual int hitItem(const QPointF& p) const = 0;
    virtual bool isDragHandle(int item, const QPointF& p) const = 0;
    virtual QPointF dragAnchor(int item) const = 0;
    virtual QRectF itemRect(int item) const = 0;
    virtual QString itemId(int item) const = 0;
    virtual QString hitBond(const QPointF&) const { return QString(); }
    virtual void hoverChanged() {}
    void paintDragLine(class QPainter& p);
    void applyIntro(class QPainter& p, int order) const;

    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void leaveEvent(QEvent*) override;
    void contextMenuEvent(QContextMenuEvent* e) override;

    int m_hover = -1;
    int m_pressItem = -1;
    bool m_pressOnHandle = false;
    QPointF m_pressPos;
    bool m_dragging = false;
    QPointF m_dragPos;
    int m_dropTarget = -1;
    bool m_intro = false;
    int m_introDelay = 0;
    QElapsedTimer m_introClock;
    class QTimer* m_introTimer = nullptr;
};

// ---- Retratos: elenco com rosto, nome e uma frase; vínculos em arcos ----
class DwPortraits : public DwBondDragBase {
    Q_OBJECT
public:
    explicit DwPortraits(QWidget* parent = nullptr);
    void setData(const QList<DwEntry>& entries, const QList<DwSection>& sections,
                 const QList<DwBond>& bonds, const QColor& accent);
    QSize sizeHint() const override { return QSize(260, m_height); }
    QSize minimumSizeHint() const override { return QSize(200, m_height); }

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    int hitItem(const QPointF& p) const override;
    bool isDragHandle(int item, const QPointF& p) const override;
    QPointF dragAnchor(int item) const override;
    QRectF itemRect(int item) const override;
    QString itemId(int item) const override;
    void hoverChanged() override { update(); }

private:
    void relayout();
    QList<DwEntry> m_entries;
    QList<DwSection> m_sections;
    QList<DwBond> m_bonds;
    QColor m_accent;
    QHash<int, QRectF> m_rows;          // índice → retângulo da linha
    int m_arcMargin = 4;
    QList<QPair<QString, qreal>> m_heads; // título da seção, y
    int m_height = 100;
};

// ---- Polaroid: quadro de cortiça, fotos tortas, barbante nos vínculos ----
class DwPolaroid : public DwBondDragBase {
    Q_OBJECT
public:
    explicit DwPolaroid(QWidget* parent = nullptr);
    void setData(const QList<DwEntry>& entries, const QList<DwBond>& bonds, const QColor& pin,
                 const QString& legendTitle);
    QSize sizeHint() const override { return QSize(260, m_height); }
    QSize minimumSizeHint() const override { return QSize(200, m_height); }

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    int hitItem(const QPointF& p) const override;
    bool isDragHandle(int item, const QPointF& p) const override;
    QPointF dragAnchor(int item) const override;
    QRectF itemRect(int item) const override;
    QString itemId(int item) const override;
    void hoverChanged() override { update(); }

private:
    void relayout();
    QPointF pinPos(int i) const;
    QList<DwEntry> m_entries;
    QList<DwBond> m_bonds;
    QColor m_pin;
    QString m_legendTitle;
    QList<QRectF> m_rects;
    int m_height = 100;
    qreal m_legendY = 0;
};

// ---- Crachás: uma linha por pessoa, foto na altura toda, papel em faixa ----
class DwBadges : public DwBondDragBase {
    Q_OBJECT
public:
    explicit DwBadges(QWidget* parent = nullptr);
    void setData(const QList<DwEntry>& entries, const QList<DwBond>& bonds, const QColor& accent);
    QSize sizeHint() const override { return QSize(260, m_height); }
    QSize minimumSizeHint() const override { return QSize(200, m_height); }

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    int hitItem(const QPointF& p) const override;
    bool isDragHandle(int item, const QPointF& p) const override;
    QPointF dragAnchor(int item) const override;
    QRectF itemRect(int item) const override;
    QString itemId(int item) const override;
    void hoverChanged() override { update(); }

private:
    void relayout();
    QList<DwEntry> m_entries;
    QList<DwBond> m_bonds;
    QColor m_accent;
    QList<QRectF> m_rects;
    int m_height = 100;
};

// ---- Galeria de rostos: grade de retratos 3x4; o mouse acende os vínculos ----
class DwGallery : public DwBondDragBase {
    Q_OBJECT
public:
    explicit DwGallery(QWidget* parent = nullptr);
    void setData(const QList<DwEntry>& entries, const QList<DwBond>& bonds, const QColor& accent);
    QSize sizeHint() const override { return QSize(260, m_height); }
    QSize minimumSizeHint() const override { return QSize(200, m_height); }

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    int hitItem(const QPointF& p) const override;
    bool isDragHandle(int item, const QPointF& p) const override;
    QPointF dragAnchor(int item) const override;
    QRectF itemRect(int item) const override;
    QString itemId(int item) const override;
    void hoverChanged() override { update(); }

private:
    void relayout();
    QList<DwEntry> m_entries;
    QList<DwBond> m_bonds;
    QColor m_accent;
    QList<QRectF> m_rects;
    int m_height = 100;
};

namespace DwPaint {
QPixmap face(const QPixmap& photo, const QString& name, const QColor& ring, int size, qreal dpr, bool round = true);
}
