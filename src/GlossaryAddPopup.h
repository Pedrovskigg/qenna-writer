#pragma once

#include <QFrame>
#include <QString>
#include <QStringList>

class QLineEdit;
class QTextEdit;
class QPushButton;
class QLabel;
class QToolButton;
class QTimer;
class GlossaryIndex;
class GlossaryStore;

// Popup de adicionar/editar termo do glossário. Chega sabendo do livro: a
// frase em que o termo aparece pela primeira vez, quantas vezes aparece e em
// quantos capítulos, e sugere o tipo (sigla, se for tudo maiúsculo). Tipo,
// definição e outras grafias são opcionais. Ctrl+Enter confirma, Esc fecha.
class GlossaryAddPopup : public QFrame
{
    Q_OBJECT
public:
    explicit GlossaryAddPopup(QWidget* parent = nullptr);

    void setIndex(GlossaryIndex* index) { m_index = index; }
    void setStore(GlossaryStore* store) { m_store = store; }

    // Novo termo, a partir do texto selecionado.
    void presentAt(const QPoint& globalAnchor, const QString& seedTerm);
    // Editar um termo que já existe (Dicionário).
    void presentEdit(const QPoint& globalAnchor, const QString& entryId);

    // Tipos: chaves gravadas no projeto (neutras de idioma) e o nome na tela.
    static QStringList categoryKeys();
    static QString categoryLabel(const QString& key);
    static QStringList parseAliases(const QString& text);

signals:
    void confirmed(QString term, QString definition, QString category, QStringList aliases);
    void saved(QString id, QString term, QString definition, QString category, QStringList aliases);
    void removeRequested(QString id);
    void goToFirstUseRequested(QString docKey, QString sentence);
    void cancelled();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void applyTheme();

private:
    void buildUi();
    void emitConfirm();
    void refreshContext();
    void setCategory(const QString& key);
    void place(const QPoint& globalAnchor);

    GlossaryIndex* m_index = nullptr;
    GlossaryStore* m_store = nullptr;
    QString m_editId;                 // vazio = adicionando
    QString m_category;
    QString m_firstDocKey, m_firstSentence;

    QLabel* m_header = nullptr;
    QToolButton* m_closeBtn = nullptr;
    QLineEdit* m_termEdit = nullptr;
    QLabel* m_context = nullptr;
    QToolButton* m_goBtn = nullptr;
    QList<QToolButton*> m_catBtns;
    QTextEdit* m_defEdit = nullptr;
    QLineEdit* m_aliasEdit = nullptr;
    QPushButton* m_okBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
    QToolButton* m_removeBtn = nullptr;
    QTimer* m_contextTimer = nullptr;
};
