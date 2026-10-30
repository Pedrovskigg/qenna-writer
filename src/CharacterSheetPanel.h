#pragma once

#include <QFont>
#include <QList>
#include <QPointer>
#include <QWidget>
#include <QString>
#include <functional>

#include "ProjectModel.h"   // CharacterSheet / SheetField (por valor)

class ElementsStore;
class SheetTemplatesStore;
class SheetPhoto;
class QScrollArea;
class QLabel;
class QLineEdit;
class QPushButton;
class QTextEdit;
class QTimer;
class QEvent;
class QFrame;

// A Ficha de Personagem estruturada, morando no lugar da página do editor (logo
// abaixo do DocHeader — ver MainWindow::positionCharacterSheet). Foto, nome,
// apelidos e papel vêm do Element vinculado; os dados (linhas curtas) e as
// seções (texto) vêm do DrawerItem.sheet.
//
// A mesma ficha tem três formas de exibição, escolhidas por ficha no "⋯" do
// DocHeader (openMenu): Dossiê (documento corrido), Infobox (caixa de dados à
// direita, estilo wiki) e Prontuário (trilho fixo + índice das seções). Os
// dados são os mesmos nas três: trocar de forma não converte nada.
//
// Edição no lugar: tudo parece texto em repouso e revela que é editável no
// hover/foco. Auto-save por debounce.
class CharacterSheetPanel : public QWidget {
    Q_OBJECT
public:
    CharacterSheetPanel(ProjectModel* model, ElementsStore* elements, QWidget* parent = nullptr);

    // A última forma escolhida: vira a das fichas que ainda não têm uma.
    static QString defaultLayout();

    // Carrega a ficha do item e (re)constrói a UI. Item precisa ser isSheet.
    void openItem(const QString& itemId);
    QString currentItemId() const { return m_itemId; }

    // Largura além da folha que o painel precisa pra barra de rolagem ficar do
    // lado de fora da página, como a do editor.
    int scrollBarRoom() const;

    // Fonte de escrita do editor (ex. Alegreya): dá a família do conteúdo, dos
    // títulos e do nome. Os rótulos ficam na fonte da UI.
    void setContentFont(const QFont& f);

    // Busca local (Ctrl+F) dentro da ficha aberta: varre os campos, destaca e navega.
    void openFind();

    // O menu "⋯" do DocHeader: forma de exibição, adicionar, modelos, busca.
    void openMenu(const QRect& anchorGlobal);

    // Store de modelos de ficha (opcional) — habilita "Usar modelo" e "Salvar como modelo".
    void setTemplatesStore(SheetTemplatesStore* store) { m_templates = store; }

    // Raiz do projeto atual — usada só pra registrar na galeria de imagens
    // geradas (GeneratedImageGallery) quando "Gerar" é usado na foto.
    void setProjectRoot(const QString& root) { m_projectRoot = root; }

    // Redesenha foto, nome, apelidos e papel a partir do Element atual —
    // público pra MainWindow atualizar a ficha aberta quando a imagem é gerada
    // por um gatilho externo (menu de contexto da gaveta, tool da Mira).
    void refreshPhoto();

signals:
    void edited();        // disparado (com debounce) quando algo muda — pede save
    void closeRequested();
    // Duplo clique no nome: abre a folha de editar personagem (nome, apelidos, papel).
    void editCharacterRequested(const QString& itemId);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    struct Colors;
    enum class FactStyle { Dossie, Row, Rail };

    QString layoutId() const;
    void setLayout(const QString& id);
    void applyStyle();                    // cores do tema atual (refeito a cada troca de tema)
    void rebuild();                       // reconstrói toda a UI a partir de m_sheet
    void buildDossie(QWidget* page);
    void buildInfobox(QWidget* page);
    void buildProntuario(QWidget* page);
    void buildFindBar();                  // cria a barra de busca local (Ctrl+F)
    void onFindChanged(const QString& q); // recalcula matches nos campos
    void applyFindHighlights();           // pinta os matches nos blocos de texto
    void gotoMatch(int idx);              // rola e seleciona o match
    void findNext();
    void findPrev();
    void closeFind();
    void scheduleSave();                  // (re)inicia o debounce de save
    void commitNow();                     // grava m_sheet no model + emite edited()
    void setFieldValue(const QString& id, const QString& value);
    void addField(const QString& kind);   // "data" | "text"
    void removeField(const QString& id);
    void moveSection(const QString& id, int targetIndex);
    void applyTemplate(const QString& templateId);
    void saveAsTemplate();
    void pickPhoto();
    void generateImage();
    void removePhoto();
    void refreshIdentity();               // nome, apelidos e papel
    void updateStickies();                // infobox, trilho e índice acompanham a rolagem
    void showToast(const QString& text, std::function<void()> undo = {});

    QWidget* buildPhoto(const QSize& size);
    QWidget* buildIdentity(int namePx, Qt::Alignment align, bool withAlias);
    QWidget* buildFact(const SheetField& f, FactStyle style);
    QWidget* buildSection(const SheetField& f);
    QPushButton* buildAdd(const QString& kind);
    QFrame* buildRule();
    QList<SheetField> fieldsOfKind(const QString& kind) const;
    QFont contentFont(int px, int weight = QFont::Normal, bool italic = false) const;

    ProjectModel* m_model;
    ElementsStore* m_elements;
    SheetTemplatesStore* m_templates = nullptr;
    QString m_itemId;
    QString m_elementId;
    QString m_projectRoot;
    CharacterSheet m_sheet;     // cópia de trabalho

    QScrollArea* m_scroll = nullptr;
    QWidget* m_content = nullptr;
    QWidget* m_page = nullptr;
    QTimer* m_saveTimer = nullptr;
    QFont m_contentFont;        // família do conteúdo (escrita)
    QString m_focusAfterRebuild;   // campo recém-criado: ganha o foco com o rótulo selecionado

    QList<QPointer<SheetPhoto>> m_photos;
    QList<QPointer<QLabel>> m_names, m_aliases, m_roles;

    // Elementos que grudam no topo ao rolar (a caixa do Infobox, o trilho e o
    // índice do Prontuário): o widget flutua por cima do "lugar" que ocupa no layout.
    struct Sticky { QPointer<QWidget> w; QPointer<QWidget> holder; int gap = 0; };
    QList<Sticky> m_stickies;

    // Arrastar seção pelo ⋮⋮
    QList<QPointer<QWidget>> m_sections;   // na ordem da tela
    QString m_dragId;
    int m_dropIndex = -1;
    QFrame* m_dropLine = nullptr;

    // Aviso no pé da ficha, com "Desfazer" quando algo foi removido
    QWidget* m_toast = nullptr;
    QLabel* m_toastText = nullptr;
    QPushButton* m_toastUndo = nullptr;
    QTimer* m_toastTimer = nullptr;
    std::function<void()> m_undo;

    // Busca local (Ctrl+F)
    QWidget* m_findBar = nullptr;
    QLineEdit* m_findInput = nullptr;
    QLabel* m_findCount = nullptr;
    QList<QWidget*> m_searchFields;    // campos buscáveis na ordem visual (QTextEdit/QLineEdit)
    QList<QTextEdit*> m_textEdits;     // subconjunto que recebe highlight de matches
    struct FindMatch { QWidget* w; int pos; int len; };
    QList<FindMatch> m_matches;
    int m_activeMatch = -1;
};
