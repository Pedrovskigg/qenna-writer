#pragma once

#include "SheetDialog.h"

#include <QString>
#include <QStringList>
#include <QVector>

#include "SheetTemplatesStore.h"   // SheetTemplate (por valor)

class CharacterPoster;
class QButtonGroup;
class QCheckBox;
class QLabel;
class QLineEdit;
class QToolButton;
class QPushButton;

// Folha de criação/edição de elemento narrativo (personagem, cenário, objeto)
// ou de documento de gaveta (tipo vazio). Ver SheetDialog: sem moldura do
// sistema, na cor da página. Personagem abre com o "pôster em pé" (faixa com
// silhueta; com foto, a foto de ponta a ponta e o nome por cima), e embaixo
// apelidos, papel, narrador, trilha na Timeline e tipo de página; cenário e
// objeto, só foto + nome; documento, só o nome.
class ElementCreateDialog : public SheetDialog {
    Q_OBJECT
public:
    // elementType: "character" | "setting" | "object" | "" (vazio = item comum)
    // sheetTemplates: modelos de ficha salvos, oferecidos quando o tipo de
    // página escolhido for "Ficha" (só personagem).
    ElementCreateDialog(const QString& elementType, QWidget* parent = nullptr,
                        const QVector<SheetTemplate>& sheetTemplates = {});

    // Pré-preencher pra modo de edição.
    void setInitial(const QString& title, const QString& role, const QString& imageDataUrl,
                    bool narrator = false, const QString& trackMode = QString(),
                    const QStringList& aliases = QStringList(), const QString& gender = QString());

    // Só o nome, sem virar modo de edição (criar a partir de um trecho
    // selecionado ou de um card da Lousa: o nome vem sugerido, mas é novo).
    void presetTitle(const QString& title);

    QString title() const;
    QString role() const;
    // true = criar a página do personagem como Ficha estruturada (só personagem).
    bool createAsSheet() const;
    // Modelo de ficha escolhido (vazio = "Vazio (padrão)", sem modelo).
    QString selectedTemplateId() const;
    QString imageDataUrl() const { return m_imageDataUrl; }
    bool narrator() const;
    QString trackMode() const;  // "" auto | "on" | "off" (trilha na linha do tempo)
    QStringList aliases() const; // apelidos do personagem (para o detector de presença)
    QString gender() const;      // "m" | "f" | "" (forma de tratamento, só personagem)

protected:
    void showEvent(QShowEvent* e) override;

private:
    void buildUi();
    void updatePreview();
    void updatePageTypeUi();
    void pickImage();

    QString m_elementType;
    QString m_imageDataUrl;
    QVector<SheetTemplate> m_sheetTemplates;
    bool m_editMode = false;

    QLineEdit*    m_titleEdit = nullptr;
    QLineEdit*    m_aliasesEdit = nullptr;
    QToolButton*  m_photoBtn = nullptr;
    QToolButton*  m_removePhoto = nullptr;
    // Papel: uma linha ("Coadjuvante · apoia os principais ▾") que abre, na
    // própria folha, a lista dos papéis com a explicação de cada um. Nada de
    // menu suspenso: janela suspensa sai com fundo transparente no Windows
    // sobre esta folha translúcida.
    QString       m_roleValue;              // "PROTAGONISTA"…, "OUTRO" ou vazio
    QPushButton*  m_rolePick = nullptr;
    QLabel*       m_rolePickText = nullptr;
    QWidget*      m_roleList = nullptr;
    QLineEdit*    m_roleCustom = nullptr;
    void setRole(const QString& id);
    void refreshRoleTag();
    QLabel*       m_roleTag = nullptr;      // o papel escrito em cima do nome, no pôster
    CharacterPoster* m_poster = nullptr;    // topo da folha de personagem (faixa ou foto)
    QCheckBox*    m_narratorCheck = nullptr;
    QButtonGroup* m_genderGroup = nullptr;  // 0 = Masculino, 1 = Feminino; nenhum = não marcado
    QButtonGroup* m_trackGroup = nullptr;
    QButtonGroup* m_pageGroup = nullptr;
    QWidget*      m_templateBox = nullptr;
    class SheetChoice* m_templateChoice = nullptr;
};
