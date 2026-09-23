#pragma once
// Modo Trança da Timeline nova: ordem de leitura (em cima) × ordem da história
// (embaixo), ligadas por fios. Flashback cruza pra trás; capítulos do mesmo
// dia caem no mesmo ponto; saltos grandes viram quebra no eixo.

#include "TimelineTracksTypes.h"

#include <QHash>
#include <QPainterPath>
#include <QWidget>

class TimelineBraidView : public QWidget {
    Q_OBJECT
public:
    explicit TimelineBraidView(QWidget* parent = nullptr);

    void setData(const Tracks::Data& data) { m_data = data; update(); }
    void setFilter(const Tracks::Filter& f) { m_filter = f; update(); }
    void setSelected(const QString& id) { m_sel = id; update(); }

signals:
    void eventClicked(const QString& id);
    void backgroundClicked();

protected:
    void paintEvent(QPaintEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void leaveEvent(QEvent* e) override;

private:
    QString ribbonAt(const QPointF& pos) const;

    Tracks::Data   m_data;
    Tracks::Filter m_filter;
    QString        m_sel;
    QString        m_hover;
    QHash<QString, QPainterPath> m_paths; // evento → fio (pra hover/clique)
};
