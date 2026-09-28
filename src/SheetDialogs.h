#pragma once
// Folhas prontas pros pedidos simples (ver SheetDialog): nome de pasta, de
// parte, de lousa, sinopse, escolha de tipo… no lugar do QInputDialog cru do
// sistema. E a folha do manuscrito.

#include <QString>
#include <QStringList>

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

} // namespace Sheets
