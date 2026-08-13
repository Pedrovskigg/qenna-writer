#pragma once

#include "TerritorioStore.h"

#include <QFrame>
#include <QPair>
#include <QString>
#include <QVector>

class QLabel;
class QComboBox;
class QPushButton;

// Popup pra salvar o trecho selecionado como uma "menção" vinculada a um
// Território do Criador de Mundos (opcionalmente a uma Pasta/Documento
// específico dentro dele) — clone de ConstrutorMentionAddPopup, com um campo
// a mais: categoria livre (guerra/criatura/batalha/...), usada depois pra
// filtrar "o que aconteceu aqui" sem precisar de parsing de texto livre.
class TerritorioMentionAddPopup : public QFrame
{
    Q_OBJECT
public:
    explicit TerritorioMentionAddPopup(QWidget* parent = nullptr);

    void setTerritorioStore(TerritorioStore* store);

    void presentAt(const QPoint& globalAnchor,
                   const QString& selectedText,
                   const QString& sourceLabel,
                   const QVector<QPair<QString, QString>>& territorios);

signals:
    // nodeId vazio = menção do território como um todo, sem nó específico.
    void confirmed(QString territorioId, QString nodeId, QString category);
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

    TerritorioStore* m_store = nullptr;

    QLabel*      m_header       = nullptr;
    QLabel*      m_sourceLabel  = nullptr;
    QLabel*      m_preview      = nullptr;
    QComboBox*   m_territorioCombo = nullptr;
    QComboBox*   m_nodeCombo    = nullptr;
    QComboBox*   m_categoryCombo = nullptr;
    QPushButton* m_okBtn        = nullptr;
    QPushButton* m_cancelBtn    = nullptr;
};
