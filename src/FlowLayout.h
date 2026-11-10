#pragma once

// Layout que quebra linha: os itens vão da esquerda pra direita e descem quando não cabem.
// Usado na fileira de pastas da gaveta (nome comprido não corta mais) e nas tags.

#include <QLayout>
#include <QList>
#include <QWidget>

class FlowLayout : public QLayout {
public:
    explicit FlowLayout(QWidget* parent, int spacing) : QLayout(parent), m_spacing(spacing) { setContentsMargins(0, 0, 0, 0); }
    ~FlowLayout() override { while (QLayoutItem* it = takeAt(0)) delete it; }
    void addItem(QLayoutItem* it) override { m_items.append(it); }
    int count() const override { return int(m_items.size()); }
    QLayoutItem* itemAt(int i) const override { return m_items.value(i); }
    QLayoutItem* takeAt(int i) override { return (i >= 0 && i < m_items.size()) ? m_items.takeAt(i) : nullptr; }
    Qt::Orientations expandingDirections() const override { return {}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override { return place(QRect(0, 0, w, 0), false); }
    void setGeometry(const QRect& r) override { QLayout::setGeometry(r); place(r, true); }
    QSize sizeHint() const override { return minimumSize(); }
    QSize minimumSize() const override
    {
        QSize s;
        for (QLayoutItem* it : m_items) if (!it->isEmpty()) s = s.expandedTo(it->minimumSize());
        const QMargins m = contentsMargins();
        return s + QSize(m.left() + m.right(), m.top() + m.bottom());
    }

private:
    int place(const QRect& rect, bool apply) const
    {
        const QMargins m = contentsMargins();
        const QRect r = rect.adjusted(m.left(), m.top(), -m.right(), -m.bottom());
        int x = r.x(), y = r.y(), line = 0;
        for (QLayoutItem* it : m_items) {
            if (it->isEmpty()) continue;   // escondido não ocupa lugar
            QSize sh = it->sizeHint();
            sh.setWidth(qMin(sh.width(), qMax(1, r.width())));
            if (x + sh.width() > r.right() + 1 && line > 0) {
                x = r.x();
                y += line + m_spacing;
                line = 0;
            }
            if (apply) it->setGeometry(QRect(QPoint(x, y), sh));
            x += sh.width() + m_spacing;
            line = qMax(line, sh.height());
        }
        return y + line - rect.y() + m.bottom();
    }
    QList<QLayoutItem*> m_items;
    int m_spacing;
};
