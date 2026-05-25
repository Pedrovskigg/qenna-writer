#pragma once

#include "ProjectModel.h"

#include <QFrame>
#include <QString>

class QLabel;
class QPushButton;
class QToolButton;
class QVBoxLayout;
class QWidget;

// Painel de leitura mostrado ao clicar numa linha de vínculo. Header com
// barra fina pra drag + ícones de ação (editar / excluir / criar documento /
// fechar). Corpo com a sentença "Ana é irmã de Maria" + backstory.
//
// Igual ao BondPopup, é arrastável pelo header e o fechamento por
// outside-click é responsabilidade do chamador.

class BondViewPanel : public QFrame {
    Q_OBJECT
public:
    BondViewPanel(QWidget* parent,
                  const CharacterBond& bond,
                  const QString& fromTitle,
                  const QString& toTitle);

signals:
    void editRequested();
    void deleteRequested();
    void createDocRequested();
    void closeRequested();

protected:
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;

private:
    void buildUi(const CharacterBond& bond,
                 const QString& fromTitle,
                 const QString& toTitle);

    QString m_color;
    QWidget* m_header = nullptr;
    bool m_dragging = false;
    QPoint m_dragOffset;
};
