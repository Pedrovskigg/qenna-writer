#pragma once

#include <QColor>
#include <QFrame>
#include <QList>
#include <QString>

class QLabel;
class QLineEdit;
class QTextEdit;
class QToolButton;
class QHBoxLayout;

// Popup pra criar/editar uma nota do Pensário: paleta de cor + caixa de texto.
// Filho do container do editor; centraliza sobre ele ao abrir. Esc cancela,
// Ctrl+Enter salva.
class NoteEditPopup : public QFrame {
    Q_OBJECT
public:
    explicit NoteEditPopup(QWidget* parent = nullptr);

    void openForCreate();
    void openForEdit(const QColor& color, const QString& title, const QString& text);

signals:
    void confirmed(QColor color, QString title, QString text);
    void cancelled();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void applyTheme();

private:
    void buildUi();
    void buildPalette();
    void applySwatchSelection();
    void centerInParent();
    void emitConfirm();

    QColor m_color;
    QLabel* m_title = nullptr;
    QLineEdit* m_titleEdit = nullptr;
    QHBoxLayout* m_paletteRow = nullptr;
    QTextEdit* m_textEdit = nullptr;
    QToolButton* m_saveBtn = nullptr;
    QToolButton* m_cancelBtn = nullptr;
    QList<QToolButton*> m_swatches;
};
