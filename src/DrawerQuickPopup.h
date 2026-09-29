#pragma once
// Nova gaveta (e Editar gaveta) como jogo rápido: um cartão estreito e em pé
// ("Coluna") que brota ao lado do "+" da LeftBar, DENTRO da janela (filho dela,
// não uma janela nova), sem véu e sem modal. Ícone grande na cor da gaveta,
// nome, os ícones mais usados à vista (o resto no "+N"), cor e tipo no pé. O
// ícone segue o nome sozinho. O tipo de elemento abre num link. Enter confirma; Esc ou clique fora fecham, e
// o cursor volta pra onde estava (o editor).

#include <QFrame>
#include <QPointer>
#include <QStringList>

class ColorWheelButton;
class ElementsStore;
class QGridLayout;
class QLabel;
class QLineEdit;
class QToolButton;
class SheetChoice;

class DrawerQuickPopup : public QFrame {
    Q_OBJECT
public:
    DrawerQuickPopup(ElementsStore* store, QWidget* window);

    // anchorGlobal: o que ele acompanha (o "+", ou o ponto do clique direito)
    void openCreate(const QRect& anchorGlobal, const QString& color);
    void openEdit(const QRect& anchorGlobal, const QString& title, const QString& icon,
                  const QString& color, const QString& elementTypeId);
    bool editing() const { return m_edit; }

signals:
    void confirmed(const QString& title, const QString& icon, const QString& color,
                   const QString& elementTypeId);

protected:
    bool eventFilter(QObject* o, QEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private:
    void open(const QRect& anchorGlobal);
    void dismiss();
    void confirm();
    void setIcon(const QString& id, bool manual);
    void refreshBadge();
    void refreshTypeLink();
    void layoutIcons();
    void applyTheme();

    ElementsStore* m_store;
    QWidget* m_window;
    QPointer<QWidget> m_prevFocus;
    bool m_edit = false;
    bool m_iconManual = false;
    QString m_icon = QStringLiteral("drawer");
    QStringList m_typeIds;

    QToolButton* m_badge = nullptr;
    QLineEdit* m_name = nullptr;
    ColorWheelButton* m_wheel = nullptr;
    QWidget* m_iconGrid = nullptr;
    QList<QToolButton*> m_iconBtns;
    class QGridLayout* m_iconLayout = nullptr;
    QToolButton* m_moreBtn = nullptr;
    bool m_allIcons = false;
    QWidget* m_typeBox = nullptr;
    SheetChoice* m_type = nullptr;
    QLabel* m_hint = nullptr;
    QToolButton* m_typeLink = nullptr;
};
