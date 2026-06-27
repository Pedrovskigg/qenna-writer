#pragma once

#include <QDialog>
#include <QList>
#include <QPair>
#include <QString>

class QCheckBox;
class QComboBox;
class QLabel;
class QSlider;
class QSpinBox;

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

    bool detectionEnabled() const;
    bool detectionMarkAll() const;
    void setDetectionEnabled(bool enabled);
    void setDetectionMarkAll(bool markAll);

    bool autoNavEnabled() const;
    void setAutoNavEnabled(bool enabled);

    int maxDocs() const;
    void setMaxDocs(int n);

    bool mentionManuscriptsEnabled() const;
    void setMentionManuscriptsEnabled(bool enabled);

    // Teto do slider de comprimento de página (px). Acima disso a folha seria
    // cortada fora da janela; no máximo ela bate exatamente na tela.
    void setPageHeightMaximum(int px);

signals:
    void spellEnabledChanged(bool enabled);
    void spellLanguageChanged(const QString& code);
    void detectionEnabledChanged(bool enabled);
    void detectionMarkAllChanged(bool markAll);
    void autoNavEnabledChanged(bool enabled);
    void maxDocsChanged(int n);
    void mentionManuscriptsEnabledChanged(bool enabled);

private:
    void onCheckToggled(bool checked);
    void onLanguageChanged(int index);
    void syncPageLayoutFromManager();
    QString pageHeightLabelText(int v) const;
    void applyTheme();

    QCheckBox* m_spellCheck;
    QComboBox* m_langCombo;
    QCheckBox* m_detectionCheck    = nullptr;
    QCheckBox* m_detectionAllCheck = nullptr;
    QCheckBox* m_autoNavCheck               = nullptr;
    QSpinBox*  m_maxDocsSpinBox             = nullptr;
    QCheckBox* m_mentionManuscriptsCheck    = nullptr;
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
