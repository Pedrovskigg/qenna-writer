#pragma once

#include <QDialog>
#include <QString>

class QListWidget;
class QListWidgetItem;
class QLabel;
class QPushButton;

// Painel flutuante de Temas — invocado pelo botão "Temas" da TopToolbar.
// MVP da trilha Making Useful: lista vertical de temas bundled (Full Black,
// Full White) + preview de cores + botão "Aplicar". Custom themes ficam pra
// uma versão futura.
class ThemesPanel : public QDialog {
    Q_OBJECT
public:
    explicit ThemesPanel(QWidget* parent = nullptr);

private slots:
    void onSelectionChanged();
    void onApplyClicked();
    void onThemeChanged();

private:
    void rebuildList();
    void rebuildPreview();
    void applyDialogStyle();

    QListWidget* m_list;
    QLabel* m_previewName;
    QLabel* m_previewSwatch;
    QLabel* m_previewHint;
    QPushButton* m_applyButton;

    QString m_selectedId;
};
