#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>

class QLineEdit;
class QComboBox;
class QPushButton;
class ElementsStore;

// Popup unificado de criação de gaveta — espelha o renameHost "drawerCreate"
// do Mira 1: nome + ícone + cor + tipo de elemento, tudo na mesma caixa.
class DrawerCreateDialog : public QDialog {
    Q_OBJECT
public:
    explicit DrawerCreateDialog(ElementsStore* store, QWidget* parent = nullptr);

    QString title() const;
    QString iconId() const;                 // id do DRAWER_ICON_OPTIONS
    QString color() const;                  // hex #rrggbb
    QString elementTypeId() const;          // "" = Automático

    static QString autoIconFromTitle(const QString& title);
    static QStringList drawerIconCatalog();
    static QString iconLabel(const QString& iconId);

private slots:
    void onNameChanged(const QString& text);
    void onPickColor();

private:
    void populateIcons();
    void populateElementTypes();
    void updateColorSwatch();

    ElementsStore* m_store;
    QLineEdit* m_nameEdit;
    QComboBox* m_iconCombo;
    QPushButton* m_colorBtn;
    QComboBox* m_typeCombo;

    QString m_color;          // hex
    bool m_iconManual = false; // se o usuário escolheu manualmente, não auto-segue
};
