#pragma once
// Folha de Novo projeto: tudo numa folha (no lugar das três janelas de antes —
// template, detalhes, pasta). À esquerda a capa, que abre a capa rápida com o
// mouse (ver QuickCover); à direita título, autor (lembrado do último
// projeto), gêneros, sinopse, livro ou roteiro e com o que o projeto começa. A
// pasta é uma linha no rodapé, lembrando a última usada. Editando a capa, o
// lado direito dá lugar aos controles dela ("troca de lado").

#include "QuickCover.h"
#include "SheetDialog.h"

#include <QJsonObject>
#include <QStringList>

class QLabel;
class QLineEdit;
class QStackedWidget;
class QTextEdit;
class QuickCoverCanvas;
class QuickCoverPanel;
class TagEdit;

class NewProjectSheet : public SheetDialog {
    Q_OBJECT
public:
    NewProjectSheet(const QStringList& fontFamilies, QWidget* parent = nullptr);

    // Editar projeto (menu de contexto do card no menu principal): a mesma
    // folha, sem "Começa" e sem a pasta (não mudam depois de criado). A capa
    // de antes aparece como está; só vira capa rápida se o autor clicar nela.
    struct Existing {
        QString name, author, genres, synopsis;
        QString cover, coverBg;      // data URLs
        QJsonObject quickCover;      // ajustes da capa rápida, se ela foi usada
    };
    void setExisting(const Existing& e);
    // true = a capa não foi mexida: grava a de antes (cover/coverBg/quickCover iguais)
    bool keepsOriginalCover() const { return m_asIs; }

    QString projectName() const;
    QString author() const;
    QString genres() const;
    QString synopsis() const;
    QString projectType() const;   // "book" | "screenplay"
    QString templateId() const;    // "blank" | "basic" | "advanced"
    QString fullPath() const;      // <pasta escolhida>/<nome do projeto>
    // A capa pronta (com texto), a mesma sem texto (textura do menu principal)
    // e os ajustes da capa rápida, pra reabrir de onde parou.
    QString coverDataUrl() const { return m_coverFull; }
    QString coverBgDataUrl() const { return m_coverBg; }
    QJsonObject quickCoverJson() const { return QuickCover::toJson(m_spec); }

public slots:
    void accept() override;

protected:
    void keyPressEvent(QKeyEvent* e) override;

private:
    void setEditing(bool on);
    void pickImage();
    void exportImage();
    void refreshPath();
    void refreshCanvasTexts();

    QuickCover::Spec m_spec;
    bool m_openedOnce = false;
    bool m_editMode = false;
    bool m_asIs = false;             // editando: a capa de antes, intocada
    Existing m_existing;
    QWidget* m_typeBox = nullptr;
    QWidget* m_templateBox = nullptr;
    QWidget* m_pathRow = nullptr;
    QuickCoverCanvas* m_canvas = nullptr;
    QuickCoverPanel* m_panel = nullptr;
    QLabel* m_canvasHint = nullptr;
    QStackedWidget* m_right = nullptr;
    QLineEdit* m_title = nullptr;
    QLineEdit* m_author = nullptr;
    QLabel* m_authorHint = nullptr;
    TagEdit* m_genres = nullptr;
    QTextEdit* m_synopsis = nullptr;
    class SheetChoice* m_type = nullptr;
    class SheetChoice* m_template = nullptr;
    QLabel* m_templateHint = nullptr;
    QLabel* m_path = nullptr;
    QString m_parentDir;
    QString m_rememberedAuthor;
    QString m_coverFull;
    QString m_coverBg;
};
