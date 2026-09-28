#pragma once

#include "SheetDialog.h"
#include <QString>
#include <QStringList>

class QButtonGroup;
class QLabel;
class QLineEdit;
class QToolButton;
class ElementsStore;

// Folha de criação/edição de gaveta (ver SheetDialog): nome grande com o
// selo do ícone na cor da gaveta ao lado, ícones em grade, cores prontas (e
// "outra…" pelo seletor do Qenna) e o tipo de elemento em pílulas.
class DrawerCreateDialog : public SheetDialog {
    Q_OBJECT
public:
    explicit DrawerCreateDialog(ElementsStore* store, QWidget* parent = nullptr);

    QString title() const;
    QString iconId() const;                 // id do DRAWER_ICON_OPTIONS
    QString color() const;                  // hex #rrggbb
    QString elementTypeId() const;          // "" = Automático

    // Modo edit — preenche valores existentes e troca textos do botão/janela.
    void configureForEdit(const QString& title,
                          const QString& iconId,
                          const QString& color,
                          const QString& elementTypeId);

    static QString autoIconFromTitle(const QString& title);
    static QStringList drawerIconCatalog();
    static QString iconLabel(const QString& iconId);

protected:
    void showEvent(QShowEvent* e) override;

private:
    void onNameChanged(const QString& text);
    void onPickColor();
    void setIcon(const QString& id);
    void setColor(const QString& hex);
    void refreshBadge();

    ElementsStore* m_store;
    QLineEdit*    m_nameEdit = nullptr;
    QLabel*       m_badge = nullptr;
    QButtonGroup* m_iconGroup = nullptr;
    QButtonGroup* m_colorGroup = nullptr;
    QButtonGroup* m_typeGroup = nullptr;
    QStringList   m_typeIds;              // id por botão do m_typeGroup

    QString m_icon = QStringLiteral("drawer");
    QString m_color = QStringLiteral("#2b79ff");
    bool m_iconManual = false; // se o usuário escolheu manualmente, não auto-segue
};
