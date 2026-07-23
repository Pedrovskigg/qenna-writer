#pragma once

#include "ConstrutorStore.h"

#include <QFrame>
#include <QPair>
#include <QString>
#include <QVector>

class QLabel;
class QComboBox;
class QPushButton;

// Popup pra salvar o trecho selecionado como uma "menção" vinculada a um
// Sistema do Construtor (opcionalmente a uma Regra/Seção específica dentro
// dele). Aparece a partir da mini-toolbar de seleção, ao lado do
// MemoryAddPopup — mas mais simples (sem nome/tags, rejeitados pelo usuário
// por ficarem "paia" demais pra esse fluxo).
class ConstrutorMentionAddPopup : public QFrame
{
    Q_OBJECT
public:
    explicit ConstrutorMentionAddPopup(QWidget* parent = nullptr);

    // Guarda a store só pra reconstruir a árvore de nós do sistema escolhido
    // sob demanda (não precisa pré-computar a árvore de todos os sistemas
    // antes de abrir o popup).
    void setConstrutorStore(ConstrutorStore* store);

    void presentAt(const QPoint& globalAnchor,
                   const QString& selectedText,
                   const QString& sourceLabel,
                   const QVector<QPair<QString, QString>>& systems);

signals:
    // nodeId vazio = menção do sistema como um todo, sem nó específico.
    void confirmed(QString systemId, QString nodeId);
    void cancelled();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void applyTheme();

private:
    void buildUi();
    void emitConfirm();
    void rebuildNodeCombo();
    void refreshOkEnabled();

    ConstrutorStore* m_store = nullptr;

    QLabel*      m_header      = nullptr;
    QLabel*      m_sourceLabel = nullptr;
    QLabel*      m_preview     = nullptr;
    QComboBox*   m_systemCombo = nullptr;
    QComboBox*   m_nodeCombo   = nullptr;
    QPushButton* m_okBtn       = nullptr;
    QPushButton* m_cancelBtn   = nullptr;
};
