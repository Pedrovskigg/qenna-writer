#pragma once

#include <QFont>
#include <QWidget>
#include <QString>

#include "ProjectModel.h"   // CharacterSheet / SheetField (por valor)

class ElementsStore;
class QScrollArea;
class QLabel;
class QVBoxLayout;
class QTimer;
class QEvent;

// Painel embutido (overlay sobre o editorContainer) que renderiza e edita uma
// Ficha de Personagem estruturada. Foto/nome/apelido vêm do Element vinculado;
// os campos (dados + blocos de texto) vêm do DrawerItem.sheet.
//
// Edição inline com "edição invisível": campos parecem texto em repouso e
// revelam que são editáveis ao foco/hover. Auto-save por debounce.
class CharacterSheetPanel : public QWidget {
    Q_OBJECT
public:
    CharacterSheetPanel(ProjectModel* model, ElementsStore* elements, QWidget* parent = nullptr);

    // Carrega a ficha do item e (re)constrói a UI. Item precisa ser isSheet.
    void openItem(const QString& itemId);
    QString currentItemId() const { return m_itemId; }

    // Fonte usada no CONTEÚDO dos campos (a fonte de escrita do editor, ex. Alegreya).
    // Os rótulos ficam na fonte da UI — o contraste dá a pegada "documento".
    void setContentFont(const QFont& f);

signals:
    void edited();        // disparado (com debounce) quando algo muda — pede save
    void closeRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void rebuild();                       // reconstrói toda a UI a partir de m_sheet
    void scheduleSave();                  // (re)inicia o debounce de save
    void commitNow();                     // grava m_sheet no model + emite edited()
    void setFieldValue(const QString& id, const QString& value);
    void addField(const QString& kind);   // "data" | "text"
    void removeField(const QString& id);
    void moveFieldColumn(const QString& id);
    void toggleColumns();
    void pickPhoto();
    void refreshPhoto();
    QWidget* buildFieldWidget(const SheetField& f);
    QWidget* buildColumn(const QString& side);

    ProjectModel* m_model;
    ElementsStore* m_elements;
    QString m_itemId;
    QString m_elementId;
    CharacterSheet m_sheet;     // cópia de trabalho

    QScrollArea* m_scroll = nullptr;
    QWidget* m_content = nullptr;
    QLabel* m_photo = nullptr;
    QTimer* m_saveTimer = nullptr;
    QFont m_contentFont;        // fonte do conteúdo (escrita); rótulos usam a da UI
};
