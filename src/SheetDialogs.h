#pragma once
// Folhas prontas pros pedidos simples (ver SheetDialog): nome de pasta, de
// parte, de lousa, sinopse, escolha de tipo… no lugar do QInputDialog cru do
// sistema. E a folha do manuscrito.

#include <QColor>
#include <QList>
#include <QPixmap>
#include <QString>
#include <QStringList>
#include <QVector>

class QWidget;

namespace Sheets {

// Um texto. eyebrow = o que se está fazendo ("Nova pasta"); placeholder =
// o que vai no campo vazio. ok = false se cancelou. multiline = campo de
// várias linhas (Ctrl+Enter confirma). allowEmpty = aceitar vazio.
QString askText(QWidget* parent, const QString& eyebrow, const QString& placeholder,
                const QString& initial, bool* ok, const QString& okText = QString(),
                bool multiline = false, bool allowEmpty = false, const QString& hint = QString());

// Escolha entre opções (pílulas). Devolve o índice, ou -1 se cancelou.
int askChoice(QWidget* parent, const QString& eyebrow, const QStringList& options,
              const QString& okText = QString());

// Manuscrito: capa (opcional), título, "quando a história começa" (opcional,
// data-base da Timeline) e sinopse (opcional). false = cancelou.
bool askManuscript(QWidget* parent, bool editMode, QString* title, QString* storyStart,
                   QString* synopsis, QString* coverDataUrl);

// Um nome + uma escolha em pílulas (categoria do sistema, gaveta de destino…).
// choice entra com a pré-seleção e sai com o índice escolhido. optionHints:
// uma linha de explicação por opção (pode faltar), trocada conforme a escolha.
QString askTextWithChoice(QWidget* parent, const QString& eyebrow, const QString& placeholder,
                          const QString& initial, const QString& choiceLabel,
                          const QStringList& options, int* choice, bool* ok,
                          const QString& okText = QString(),
                          const QStringList& optionHints = QStringList());

// Uma cor da paleta (bolinhas). color entra com a pré-seleção e sai com a escolhida.
bool askColor(QWidget* parent, const QString& eyebrow, const QString& label,
              const QList<QColor>& palette, QColor* color, const QString& okText = QString());
// Nome + cor (grupo da gaveta).
QString askTextWithColor(QWidget* parent, const QString& eyebrow, const QString& placeholder,
                         const QList<QColor>& palette, QColor* color, bool* ok,
                         const QString& okText = QString());

// Escolher um item de uma lista com busca (documento, personagem…).
struct PickItem {
    QString label;
    QString sub;      // em cinza, à direita (a gaveta)
    QPixmap photo;    // opcional: vira bolinha à esquerda
};
constexpr int kPickExtra = -2;
// Devolve o índice escolhido, -1 se cancelou, kPickExtra se clicou no botão a
// mais do rodapé (extraText, ex.: "+ Novo personagem").
int askPick(QWidget* parent, const QString& eyebrow, const QString& searchPlaceholder,
            const QVector<PickItem>& items, const QString& okText = QString(),
            const QString& extraText = QString(), const QString& emptyText = QString());

// Confirmação: um texto e, se tiver, um aviso em destaque embaixo.
// cancelText troca o "Cancelar" (ex.: "Manter vinheta").
bool confirm(QWidget* parent, const QString& eyebrow, const QString& text,
             const QString& warning, const QString& okText,
             const QString& cancelText = QString());

// Alterações não salvas: Salvar / Descartar / Cancelar, no desenho das folhas.
enum class SaveChoice { Save, Discard, Cancel };
SaveChoice askSaveChanges(QWidget* parent, const QString& text);

} // namespace Sheets
