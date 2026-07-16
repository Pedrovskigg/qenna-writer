#pragma once

#include <QDialog>

class QPushButton;

class AboutDialog : public QDialog {
    Q_OBJECT
public:
    explicit AboutDialog(QWidget* parent = nullptr);
private:
    void applyTheme();
    QPushButton* m_emailBtn = nullptr;
};
