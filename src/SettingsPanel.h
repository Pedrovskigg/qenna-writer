#pragma once

#include <QDialog>
#include <QList>
#include <QPair>
#include <QString>

class QCheckBox;
class QComboBox;
class QLabel;
class QSlider;

class SettingsPanel : public QDialog {
    Q_OBJECT
public:
    explicit SettingsPanel(QWidget* parent = nullptr);

    // Popula a lista de idiomas disponíveis. {code, label}.
    void setAvailableSpellLanguages(const QList<QPair<QString, QString>>& langs);
    void setSpellEnabled(bool enabled);
    void setSpellLanguage(const QString& code);

    bool spellEnabled() const;
    QString spellLanguage() const;

signals:
    void spellEnabledChanged(bool enabled);
    void spellLanguageChanged(const QString& code);

private:
    void onCheckToggled(bool checked);
    void onLanguageChanged(int index);
    void syncPageLayoutFromManager();
    void applyTheme();

    QCheckBox* m_spellCheck;
    QComboBox* m_langCombo;
    QSlider* m_pageWidthSlider = nullptr;
    QSlider* m_pageHeightSlider = nullptr;
    QSlider* m_hMarginSlider = nullptr;
    QSlider* m_vMarginSlider = nullptr;
    QLabel* m_pageWidthValue = nullptr;
    QLabel* m_pageHeightValue = nullptr;
    QLabel* m_hMarginValue = nullptr;
    QLabel* m_vMarginValue = nullptr;
    QLabel* m_spellHint = nullptr;
    QLabel* m_pageHint = nullptr;
    bool m_blockSignals = false;
    bool m_blockLayoutSignals = false;
};
