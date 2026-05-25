#pragma once

#include <QColor>
#include <QFrame>
#include <QString>

class QLineEdit;
class QTextEdit;
class QToolButton;
class QHBoxLayout;
class QVBoxLayout;

// Popup unificado pra escolher cor de marcador, com modo opcional pra
// comentário (mostra textarea). Sem rouba-foco do editor quando em modo cor;
// em modo comentário, foca o textarea.
class MarkerPickPopup : public QFrame
{
    Q_OBJECT
public:
    enum Mode { ColorOnly, WithComment };

    explicit MarkerPickPopup(QWidget* parent = nullptr);

    void setMode(Mode m);
    Mode mode() const { return m_mode; }

    void setColor(const QColor& c);
    QColor color() const { return m_color; }

    void setComment(const QString& text);
    QString comment() const;

    // Posiciona o popup acima do retângulo da seleção em coordenadas globais.
    void showAbove(const QRect& anchorGlobal);

signals:
    void confirmed(QColor color, QString comment);
    void cancelled();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private slots:
    void applyTheme();

private:
    void buildUi();
    void rebuildPalette();
    void rebuildLayout();
    void applySwatchSelection();
    void openCustomColorDialog();
    void emitConfirm();

    Mode m_mode = ColorOnly;
    QColor m_color;
    QString m_pendingComment;

    QVBoxLayout* m_root = nullptr;
    QHBoxLayout* m_paletteRow = nullptr;
    QHBoxLayout* m_actionsRow = nullptr;
    QWidget* m_commentArea = nullptr;
    QTextEdit* m_commentEdit = nullptr;
    QToolButton* m_customBtn = nullptr;
    QToolButton* m_confirmBtn = nullptr;
    QToolButton* m_cancelBtn = nullptr;
    QList<QToolButton*> m_swatches;

    bool m_dragging = false;
    QPoint m_dragOffset;
};
