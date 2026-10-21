#pragma once

// O anel de ferramentas: abre em volta do cursor (botão direito no mapa) ou da
// pílula da ferramenta. Cada fatia é uma ferramenta; clicar fora fecha.

#include <QString>
#include <QVector>
#include <QWidget>

namespace carto {

struct RingItem {
    QString id;
    QString label;
    QString icon;      // recurso SVG
    bool enabled = true;
};

class Ring : public QWidget {
    Q_OBJECT
public:
    explicit Ring(QWidget* parent);
    void setItems(const QVector<RingItem>& items) { m_items = items; }
    // abre centrado em `at` (coordenadas do pai); `active` = fatia em destaque; centro mostra o rótulo
    void openAt(QPoint at, const QString& active, const QString& centerTitle, const QString& centerSub);
    QPoint center() const { return m_c; }

signals:
    void picked(const QString& id, QPoint center);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    bool eventFilter(QObject* o, QEvent* e) override;

private:
    int sliceAt(QPointF p) const;
    QVector<RingItem> m_items;
    QPoint m_c;
    QString m_active, m_title, m_sub;
    int m_hover = -1;
};

} // namespace carto
