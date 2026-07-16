#pragma once

#include <QFrame>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

class QLabel;
class QLineEdit;
class QComboBox;
class QPushButton;
class QWidget;

// Popup pra salvar o trecho selecionado como memória. Aparece a partir da
// mini-toolbar de seleção. Deixa escolher o destino (Projeto ou um Personagem),
// dar um nome opcional e marcar tags livres (com sugestões das já usadas no
// projeto). Mostra um preview do trecho e o rótulo da fonte (capítulo/cena de
// onde veio). Modelado no GlossaryAddPopup.
class MemoryAddPopup : public QFrame
{
    Q_OBJECT
public:
    explicit MemoryAddPopup(QWidget* parent = nullptr);

    // characters: lista de (elementId, nome) dos personagens do projeto.
    // existingTags: tags já usadas no projeto, viram chips de sugestão clicáveis.
    void presentAt(const QPoint& globalAnchor,
                   const QString& selectedText,
                   const QString& sourceLabel,
                   const QVector<QPair<QString, QString>>& characters,
                   const QStringList& existingTags = {});

signals:
    // targetType: "project" | "character". elementId só vale p/ "character".
    void confirmed(QString name, QString targetType, QString elementId, QStringList tags);
    void cancelled();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void applyTheme();

private:
    void buildUi();
    void emitConfirm();
    void refreshCharVisibility();
    void refreshOkEnabled();
    void rebuildTagSuggestions();
    void appendTag(const QString& tag);

    QLabel* m_header = nullptr;
    QLabel* m_sourceLabel = nullptr;
    QLabel* m_preview = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QComboBox* m_targetCombo = nullptr;
    QLabel* m_charLabel = nullptr;
    QComboBox* m_charCombo = nullptr;
    QLineEdit* m_tagsEdit = nullptr;
    QWidget* m_tagSuggestWrap = nullptr;
    QStringList m_existingTags;
    QPushButton* m_okBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
};
