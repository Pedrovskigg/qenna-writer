#pragma once

#include "Theme.h"

#include <QDialog>
#include <QList>
#include <QPointer>
#include <QString>

class QCheckBox;
class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QTabWidget;
class QTimeEdit;
class QVBoxLayout;
class QWidget;

// Painel de Temas — estilo FocusWriter. Tabs Padrão / Personalizado, cada
// tab com grid de cards visuais. Ações: Novo, Duplicar, Editar, Excluir
// (Editar/Excluir só na aba Personalizado). Footer: Aplicar + Fechar.
class ThemesPanel : public QDialog {
    Q_OBJECT
public:
    explicit ThemesPanel(QWidget* parent = nullptr);

private slots:
    void onApplyClicked();
    void onNewClicked();
    void onDuplicateClicked();
    void onEditClicked();
    void onDeleteClicked();
    void onTabChanged(int index);
    void onThemeChanged();
    void onCustomThemesChanged();
    void onAutoSwitchToggled(bool checked);
    void onDayRoleToggled(bool checked);
    void onNightRoleToggled(bool checked);
    void onAutoSwitchTimeChanged();
    void onAutoSwitchConfigChanged();
    void onSearchTextChanged(const QString& text);
    void onFavoritesChanged();

private:
    enum Tab { TabBundled = 0, TabCustom = 1 };

    void buildUi();
    QWidget* buildSearchRow();
    QWidget* buildCategoryFilterRow(QWidget* parent);
    QWidget* buildAutoSwitchRow();
    void rebuildGrids();
    void rebuildOneGrid(QWidget* gridContainer, const QList<Theme::MiraTheme>& themes, bool custom);
    void selectId(const QString& id);
    void updateButtonsState();
    void refreshAutoSwitchUi();
    void applyDialogStyle();

    Tab activeTab() const;
    bool selectedIsCustom() const;

    QTabWidget* m_tabs;
    QScrollArea* m_bundledScroll;
    QScrollArea* m_customScroll;
    QWidget* m_bundledGrid;
    QWidget* m_customGrid;

    QPushButton* m_applyButton;
    QPushButton* m_newButton;
    QPushButton* m_duplicateButton;
    QPushButton* m_editButton;
    QPushButton* m_deleteButton;
    QPushButton* m_closeButton;

    QLabel* m_selectionInfo;

    QLineEdit* m_searchEdit;
    QString m_searchText; // lowercase, pra filtro por nome nas duas abas

    // Troca automática por horário (dia/noite) — ver Theme::AutoSwitchConfig.
    QCheckBox* m_autoSwitchCheck;
    QWidget* m_autoSwitchBody;
    QCheckBox* m_dayRoleCheck;
    QCheckBox* m_nightRoleCheck;
    QLabel* m_dayThemeLabel;
    QLabel* m_nightThemeLabel;
    QTimeEdit* m_dayStartEdit;
    QTimeEdit* m_nightStartEdit;
    bool m_syncingAutoSwitchUi = false; // evita reentrância ao setar valores dos widgets programaticamente

    QString m_selectedId;
    // Filtro de categoria da aba Padrão: "all"|"light"|"warm"|"dark"|"colorful".
    QString m_categoryFilter = QStringLiteral("all");
    QList<QPointer<QFrame>> m_cards; // todos os cards visíveis (ambas as abas)
};
