#pragma once

#include "Theme.h"

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;

// Perguntado uma vez, antes de gravar um .qtheme: quem fez o tema e o que pode
// ser feito com ele.
//
// Parece burocracia pra um arquivo de cores, mas é o que separa um tema que
// circula de um tema órfão: sem autor gravado dentro do arquivo, daqui a alguns
// anos ninguém sabe de quem é nenhum dos temas que baixou — e não há como dar
// crédito, nem contato, nem (num dia futuro) pagar quem fez.
//
// Autor, contato e licença ficam lembrados entre exportações: a pessoa digita
// uma vez, não toda vez.
class ThemeExportDialog : public QDialog {
    Q_OBJECT
public:
    explicit ThemeExportDialog(const Theme::MiraTheme& theme, QWidget* parent = nullptr);

    // O tema com os metadados preenchidos. Só faz sentido após exec() == Accepted.
    const Theme::MiraTheme& theme() const { return m_theme; }

private slots:
    void onAccept();

private:
    void buildUi();
    void applyDialogStyle();
    void loadRemembered();
    void saveRemembered() const;

    Theme::MiraTheme m_theme;

    QLineEdit* m_authorEdit;
    QLineEdit* m_contactEdit;
    QComboBox* m_licenseCombo;
    QLineEdit* m_descriptionEdit;
};
