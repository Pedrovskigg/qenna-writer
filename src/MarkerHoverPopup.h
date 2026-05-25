#pragma once

#include <QColor>
#include <QFrame>
#include <QString>

class QLabel;
class QToolButton;
class QTimer;

// Popup que aparece quando o mouse paira sobre um trecho com marcador comentado.
// Mostra o comentário, com botão de excluir o marcador. Auto-fecha quando o
// mouse sai, com pequeno delay pra não piscar quando o cursor transita.
class MarkerHoverPopup : public QFrame
{
    Q_OBJECT
public:
    explicit MarkerHoverPopup(QWidget* parent = nullptr);

    void showFor(const QString& markerId,
                 const QColor& color,
                 const QString& comment,
                 const QRect& anchorGlobal);
    void scheduleClose();
    void cancelClose();
    QString currentMarkerId() const { return m_id; }

signals:
    void deleteRequested(QString markerId);
    void editRequested(QString markerId);

protected:
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private slots:
    void applyTheme();

private:
    void buildUi();
    void positionAbove(const QRect& anchorGlobal);

    QString m_id;
    QColor m_color;

    QLabel* m_text = nullptr;
    QToolButton* m_editBtn = nullptr;
    QToolButton* m_deleteBtn = nullptr;
    QTimer* m_closeTimer = nullptr;
};
