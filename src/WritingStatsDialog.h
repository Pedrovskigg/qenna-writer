#pragma once

#include <QDialog>

class WordCounter;

class WritingStatsDialog : public QDialog {
    Q_OBJECT
public:
    explicit WritingStatsDialog(WordCounter* counter, QWidget* parent = nullptr);

private:
    void buildUi();

    WordCounter* m_counter;
};
