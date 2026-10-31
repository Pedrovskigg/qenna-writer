#pragma once
// "Folha" de capítulo/cena: a janela de Novo capítulo, Editar capítulo, Nova
// cena e Editar cena. Uma folha na cor da página do editor, sem moldura do
// sistema, com o título grande e centralizado (do jeito que vai ficar no topo
// do texto), o tipo em pílulas, "quando se passa" com a leitura ao vivo e os
// atalhos de tempo, e o resumo. Os dois últimos são opcionais e dizem isso.
// Enquanto aberta, a janela do app escurece por baixo.

#include "PovPicker.h"
#include "ProjectModel.h"
#include "SheetDialog.h"
#include <QList>
#include <QString>
#include <functional>

class QCheckBox;
class QLabel;
class QLineEdit;
class QTextEdit;
class QToolButton;
class QPushButton;

struct ChapterSheetSpec {
    enum Kind { Chapter, Scene };
    Kind    kind = Chapter;
    bool    editMode = false;

    // valores (entrada e saída)
    QString title, marker, summary;
    QString type = QStringLiteral("chapter");   // só capítulo
    QString typeLabel;                           // tipo "custom"
    bool    showPov = false;  bool povOther = false;
    // Quem narra (só capítulo): etiqueta no topo da folha, com o elenco. Sem
    // personagem nenhum (povOptions vazio), nem aparece. Escolher alguém que
    // não é o narrador da obra (narratorId) já marca o "outro POV".
    QString pov;
    QList<PovPicker::Option> povOptions;
    QString narratorId;
    bool    showOptOut = false; bool optOut = false;   // popup da cena criada com "----"
    bool    openAfter = true; // saída: Enter = criar e abrir; Ctrl+Enter = criar e ficar

    // contexto
    int     position = 0;          // número do capítulo na gaveta (1-based); 0 = não mostra
    QString titlePlaceholder;      // cena: "Cena 3"
    // capítulo: rótulo que ele leva com o título vazio, pro tipo/rótulo dados
    std::function<QString(const QString& type, const QString& typeLabel)> previewLabel;
    QString prevMarker;            // marcador do capítulo/cena anterior (atalhos "=" e "+1 dia")
    QString inheritMarker;         // cena: marcador do capítulo (o que ela herda)
    QString inheritSummary;        // cena: resumo do capítulo
    QString startMarker;           // data-base do manuscrito (decide o Flashback na leitura)
    // Fundo da folha: o próprio (entrada e saída) e o herdado (do livro, pro
    // capítulo; do capítulo, pra cena). Saída: useForAll = virou o fundo do
    // livro; clearInherited = "tirar do livro".
    bool           showBackground = false;
    PageBackground background;
    PageBackground inheritBackground;
    QString        inheritFrom;   // de onde vem o herdado: "book", "theme" ou "chapter"
    bool           useForAll = false;
    bool           clearInherited = false;
};

// Abre a folha (modal). false = cancelou.
bool runChapterSheet(QWidget* parent, ChapterSheetSpec& spec);

class ChapterSheet : public SheetDialog {
    Q_OBJECT
public:
    ChapterSheet(ChapterSheetSpec& spec, QWidget* parent);

protected:
    bool eventFilter(QObject* o, QEvent* e) override;
    void showEvent(QShowEvent* e) override;

private:
    void build();
    void refreshTitle();
    void refreshReading();
    void finish(bool openAfter);
    QString currentType() const;
    void syncPovOther();
    PovPicker::Option povOption(const QString& id) const;

    ChapterSheetSpec& s;
    class SheetChoice* m_typeChoice = nullptr;   // capítulo, prólogo, interlúdio…
    QLineEdit*   m_custom = nullptr;
    QLineEdit*   m_title = nullptr;
    QLabel*      m_preview = nullptr;
    QLineEdit*   m_marker = nullptr;
    QLabel*      m_reading = nullptr;
    QToolButton* m_same = nullptr;
    QToolButton* m_plus = nullptr;
    QTextEdit*   m_summary = nullptr;
    QToolButton* m_povAdd = nullptr;
    QCheckBox*   m_pov = nullptr;
    QCheckBox*   m_optOut = nullptr;
    class PageBgField* m_bgField = nullptr;
    PovPicker*   m_povPick = nullptr;
    QString      m_povId;
    qreal        m_startChrono = 0.0;
    bool         m_startOk = false;
};
