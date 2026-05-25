#pragma once

#include "ProjectModel.h"

#include <QFrame>
#include <QString>

class QLabel;
class QLineEdit;
class QPushButton;
class QTextEdit;
class QToolButton;
class QVBoxLayout;

// Popup flutuante (sem barra de título nativa) pra criar ou editar um
// vínculo entre dois personagens. Layout 1:1 com BondPopup do Mira 1:
// dois nomes "Ana ↔ João" no topo, dropdown de tipo (com toggle M/F + texto
// livre), textarea de backstory, paleta de cor + presets, e ações.
//
// Arrastável pelo header. Fecha em outside-click (delegado ao chamador via
// signal closeRequested).

class BondPopup : public QFrame {
    Q_OBJECT
public:
    enum Mode { Create, Edit };

    BondPopup(QWidget* parent,
              Mode mode,
              const QString& fromTitle,
              const QString& toTitle,
              const CharacterBond& initial /* used only in Edit */);

    QString bondType() const { return m_type; }
    QString description() const;
    QString color() const { return m_color; }

signals:
    void confirmed(QString type, QString description, QString color);
    void deleteRequested();
    void closeRequested();

protected:
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void paintEvent(QPaintEvent* e) override;

private:
    void buildUi();
    void rebuildTypeMenuColors();
    void openTypePicker();
    void openColorPicker();
    void applyColor(const QString& c);
    void applyType(const QString& t);

    Mode m_mode;
    QString m_fromTitle;
    QString m_toTitle;
    QString m_type;
    QString m_color;

    QLabel* m_title = nullptr;
    QLabel* m_namesLabel = nullptr;
    QPushButton* m_typeButton = nullptr;
    QTextEdit* m_description = nullptr;
    QPushButton* m_colorMain = nullptr;
    QList<QPushButton*> m_colorSwatches;
    QPushButton* m_confirmBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
    QPushButton* m_deleteBtn = nullptr;
    QToolButton* m_closeBtn = nullptr;
    QWidget* m_header = nullptr;

    bool m_dragging = false;
    QPoint m_dragOffset;
};
