#pragma once

#include "Theme.h"

#include <QDialog>
#include <QList>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QSpinBox;
class ThemePreviewWidget;

// Editor de tema custom — modal, estilo FocusWriter. Coluna esquerda:
// nome + grupos de campos. Coluna direita: preview ao vivo do mini-app
// renderizado com o tema em edição. As mudanças refletem no preview a
// cada interação (sem precisar dar OK).
class ThemeEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit ThemeEditorDialog(const Theme::MiraTheme& base, QWidget* parent = nullptr);

    const Theme::MiraTheme& theme() const { return m_theme; }

private slots:
    void onPickImage();
    void onClearImage();
    void onPickColor();
    void onShadowColorClicked();

private:
    void buildUi();
    void wireSignals();
    void applyDialogStyle();

    void refreshPreview();
    void refreshImageLabel();
    void refreshSwatch(QPushButton* btn, const QString& colorStr);

    QWidget* makeColorRow(const QString& fieldKey, const QString& label);
    QString colorOf(const QString& fieldKey) const;
    void setColorIn(const QString& fieldKey, const QString& value);

    Theme::MiraTheme m_theme;

    QLineEdit* m_nameEdit;
    QList<QPushButton*> m_colorSwatches;

    QCheckBox* m_shadowEnabled;
    QPushButton* m_shadowColorSwatch;
    QSpinBox* m_shadowRadius;
    QSpinBox* m_shadowOffset;

    QLabel* m_imagePathLabel;
    QPushButton* m_imagePickBtn;
    QPushButton* m_imageClearBtn;
    QComboBox* m_imageModeCombo;

    QSlider* m_opacitySlider;
    QLabel* m_opacityLabel;

    ThemePreviewWidget* m_preview;
};
