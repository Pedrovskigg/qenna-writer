#pragma once

#include "Theme.h"

#include <QDialog>
#include <QHash>
#include <QList>
#include <QString>

class QAbstractButton;
class QAction;
class QCheckBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QTimeEdit;
class QTimer;
class QToolButton;
class QWidget;

namespace ThemesPanelDetail {
class RailItem;
class ThemeGridView;
class ComparePreview;
class SimilarStrip;
class Dot;
}

// Painel de Temas ("Silenciosa", 2026-09-26). Três colunas:
//   trilho (busca, categorias com contagem, Meus temas, Dia e noite)
//   grade de miniaturas pintadas (uma só widget, sem um widget por tema)
//   prévia da janela do Qenna dividida entre o tema em uso e o escolhido,
//   com contraste, "Parecidos com este" e Aplicar + ⋯.
// Passar o mouse na grade mostra o tema na prévia; clicar seleciona;
// duplo clique aplica. O ⤢ da prévia amplia a comparação por cima do painel.
class ThemesPanel : public QDialog {
    Q_OBJECT
public:
    explicit ThemesPanel(QWidget* parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void onApplyClicked();
    void onNewClicked();
    void onDuplicateClicked();
    void onEditClicked();
    void onDeleteClicked();
    void onExportClicked();
    void onImportClicked();
    void onThemeChanged();
    void onCustomThemesChanged();
    void onFavoritesChanged();
    void onAutoSwitchToggled(bool checked);
    void onDayRoleToggled(bool checked);
    void onNightRoleToggled(bool checked);
    void onAutoSwitchTimeChanged();
    void onAutoSwitchConfigChanged();
    void onSearchTextChanged(const QString& text);

private:
    QWidget* buildRail();
    QWidget* buildGridPage();
    QWidget* buildDayNightPage();
    QWidget* buildSide();
    QWidget* buildFooter();
    void buildZoom();

    void setView(const QString& key);
    void rebuildGrid();
    void refreshCounts();
    void refreshSide();
    void refreshFooter();
    void refreshAutoSwitchUi();
    void applyStyle();
    void setScene(int scene);
    void setCut(qreal cut);
    void setZoomVisible(bool on);
    void selectId(const QString& id);
    void onGridHover(const QString& id);
    void showMoreMenu();
    void showThemeIntroToast(const QString& text);

    QString shownId() const;
    const Theme::MiraTheme* themeById(const QString& id) const;

    // Estado
    QString m_view = QStringLiteral("all");   // all|favorites|light|warm|dark|colorful|estampados|mine|daynight
    QString m_searchText;
    QString m_selectedId;
    QString m_hoverId;
    int m_scene = 0;
    qreal m_cut = 0.5;

    // Trilho
    QLineEdit* m_searchEdit = nullptr;
    QAction* m_searchIcon = nullptr;
    bool m_scrolledToCurrent = false;
    QHash<QString, ThemesPanelDetail::RailItem*> m_railItems;

    // Meio
    QStackedWidget* m_middle = nullptr;
    QScrollArea* m_gridScroll = nullptr;
    ThemesPanelDetail::ThemeGridView* m_grid = nullptr;
    QTimer* m_hoverTimer = nullptr;

    // Lado
    QList<QPushButton*> m_sceneTabs;
    ThemesPanelDetail::ComparePreview* m_preview = nullptr;
    QLabel* m_nameLabel = nullptr;
    QToolButton* m_favButton = nullptr;
    QLabel* m_recBadge = nullptr;
    ThemesPanelDetail::Dot* m_dotText = nullptr;
    ThemesPanelDetail::Dot* m_dotMuted = nullptr;
    QLabel* m_contrastText = nullptr;
    QLabel* m_contrastTextUsing = nullptr;
    QLabel* m_contrastMuted = nullptr;
    QLabel* m_contrastMutedUsing = nullptr;
    ThemesPanelDetail::SimilarStrip* m_similar = nullptr;
    QPushButton* m_applyButton = nullptr;
    QToolButton* m_moreButton = nullptr;

    // Rodapé
    QLabel* m_usingLabel = nullptr;
    QPushButton* m_importButton = nullptr;
    QPushButton* m_closeButton = nullptr;

    // Zoom da prévia
    QWidget* m_zoom = nullptr;
    QList<QPushButton*> m_zoomTabs;
    QLabel* m_zoomNames = nullptr;
    ThemesPanelDetail::ComparePreview* m_zoomPreview = nullptr;

    // Dia e noite (conteúdo de antes, agora como página do trilho)
    QCheckBox* m_autoSwitchCheck = nullptr;
    QWidget* m_autoSwitchBody = nullptr;
    QCheckBox* m_dayRoleCheck = nullptr;
    QCheckBox* m_nightRoleCheck = nullptr;
    QLabel* m_dayThemeLabel = nullptr;
    QLabel* m_nightThemeLabel = nullptr;
    QLabel* m_roleHint = nullptr;
    QTimeEdit* m_dayStartEdit = nullptr;
    QTimeEdit* m_nightStartEdit = nullptr;
    bool m_syncingAutoSwitchUi = false;
};
