#pragma once

#include <QFrame>
#include <QString>

class QLineEdit;
class QTextEdit;
class QPushButton;
class QLabel;

// Popup compacto pra adicionar um termo ao glossário a partir do context menu
// do editor. Termo vem pré-preenchido com o texto selecionado; definição é
// opcional. Enter no termo move pra definição, Ctrl+Enter confirma.
class GlossaryAddPopup : public QFrame
{
    Q_OBJECT
public:
    explicit GlossaryAddPopup(QWidget* parent = nullptr);

    void presentAt(const QPoint& globalAnchor, const QString& seedTerm);

signals:
    void confirmed(QString term, QString definition);
    void cancelled();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void applyTheme();

private:
    void buildUi();
    void emitConfirm();

    QLabel* m_header = nullptr;
    QLineEdit* m_termEdit = nullptr;
    QTextEdit* m_defEdit = nullptr;
    QPushButton* m_okBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
};
