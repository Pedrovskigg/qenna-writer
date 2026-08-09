#pragma once

#include <QDialog>
#include <QHash>
#include <QString>
#include <QStringList>

class QLineEdit;
class QSlider;
class QPlainTextEdit;
class QPushButton;

// Criador de personalidade da Mira — nome, chips de traço combináveis
// (novo) + os sliders de calor/dureza e o texto livre que já existiam em
// Settings (mesmas chaves de QSettings; as duas telas ficam em sincronia
// automática, não são cópias independentes). Aberto clicando no próprio
// avatar/nome da Mira no cabeçalho do AIChatPanel, em modo janela — ver
// AIChatPanel::buildUi().
//
// Diferente do ImageStylePreset (CharacterImageGenService.h), que é seleção
// única, os traços aqui são combináveis: cada chip ligado soma seu próprio
// fragmento de prompt (ver miraTraitDefs()/miraTraitsFragment() em
// MiraPersonality.h) — é a primeira vez desse padrão de "múltiplos chips
// somáveis" no app.
class MiraPersonalityDialog : public QDialog {
    Q_OBJECT
public:
    explicit MiraPersonalityDialog(QWidget* parent = nullptr);

signals:
    // Emitido quando o nome muda — quem abriu o diálogo (AIChatPanel) usa
    // isso pra atualizar o avatar/título já desenhados na tela.
    void nameChanged(const QString& newName);

private:
    void applyDialogStyle();
    void toggleTrait(const QString& id, QPushButton* chip);

    QLineEdit* m_nameEdit = nullptr;
    QSlider* m_warmthSlider = nullptr;
    QSlider* m_harshnessSlider = nullptr;
    QPlainTextEdit* m_freeformEdit = nullptr;
    QHash<QString, QPushButton*> m_traitButtons;
    QStringList m_selectedTraits;
};
