#pragma once

#include "RenameService.h"

#include <QDialog>
#include <QList>

class QCheckBox;
class QVBoxLayout;

// Preview da renomeação de um elemento no projeto inteiro.
//
// As ocorrências vinculadas (@menção, que carrega o id do item no href) são
// mostradas só como contagem: não há o que conferir. As soltas — nome escrito na
// prosa — vêm agrupadas por lugar e termo, porque um personagem recorrente
// aparece às centenas e uma lista item a item seria inusável.
class RenameDialog : public QDialog {
    Q_OBJECT
public:
    explicit RenameDialog(const RenameService::Plan& plan, QWidget* parent = nullptr);

    RenameService::Plan resultPlan() const;

private:
    // Ocorrências do mesmo termo no mesmo lugar viram uma linha só.
    struct Group {
        QString where;
        QString matched;
        QString replacement;
        QString sample;      // um trecho de exemplo
        QList<int> indexes;  // posições dentro de m_plan.occurrences
        bool fromAlias = false;
        bool selected = true;
    };

    void buildUi();
    void collectGroups();
    void addGroupRows(QVBoxLayout* into, bool aliasSection);

    RenameService::Plan m_plan;
    QList<Group> m_groups;
    QList<QCheckBox*> m_checks; // paralelo a m_groups (nullptr quando não exibido)
    QCheckBox* m_aliasKeepCheck = nullptr;
};
