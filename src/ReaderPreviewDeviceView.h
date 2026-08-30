#pragma once

#include <QColor>
#include <QWidget>

class QTextDocument;

// Simula a tela de um e-reader (moldura + navegação por página, sem scroll).
// Não é dono do QTextDocument — quem chama setDocument() mantém a posse.
// Posição é sempre expressa como fração (0.0–1.0) do total de páginas, nunca
// como índice absoluto: sobrevive a resize (que muda o número de páginas) e
// é o mesmo valor usado pelo modo janela (que não pagina, só rola).
class ReaderPreviewDeviceView : public QWidget {
    Q_OBJECT
public:
    explicit ReaderPreviewDeviceView(QWidget* parent = nullptr);

    void setDocument(QTextDocument* doc);
    void setPositionFraction(double frac);
    double positionFraction() const;

    void setTextColor(const QColor& color);
    void setBackgroundColor(const QColor& color);

    int pageCount() const { return m_pageCount; }
    int currentPage() const { return m_currentPage; }

public slots:
    void nextPage();
    void previousPage();

signals:
    void positionChanged(double frac);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    QRectF computeScreenRect() const;
    void recomputePagination();
    void goToPage(int page);

    QTextDocument* m_doc = nullptr;
    int m_currentPage = 0;
    int m_pageCount = 0;
    double m_positionFraction = 0.0;
    QColor m_textColor = QColor(0x1a, 0x1a, 0x1a);
    QColor m_backgroundColor = QColor(0xf5, 0xf0, 0xe6);
};
