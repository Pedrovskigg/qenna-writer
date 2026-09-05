#pragma once

#include <QString>
#include <QVector>

// Lixeira GLOBAL de projetos (mesmo espírito de MiraUserMemoryStore: namespace
// de funções livres, sem estado em memória, lê/grava direto em
// QStandardPaths::AppDataLocation — não fica dentro da pasta de cada
// projeto). Existe porque até 2026-09-03 "Excluir projeto" fazia
// QDir::removeRecursively() na hora, sem chance de voltar atrás; um usuário
// perdeu manuscritos inteiros apagando a pasta por fora do app e recuperando
// só fragmentos ilegíveis do disco. A partir de agora nenhuma exclusão de
// projeto é definitiva por padrão: o projeto é movido pra cá, e só vira
// perda de verdade se o usuário esvaziar a lixeira manualmente (purgeProject).
//
// Layout em disco:
//   <AppData>/Trash/index.json           — metadados de cada entrada
//   <AppData>/Trash/Projects/<id>/...    — pasta do projeto movida, intacta
struct TrashedProjectEntry {
    QString id;             // também é o nome da subpasta em Trash/Projects/
    QString originalPath;   // onde o projeto vivia antes de ser excluído
    QString displayName;    // nome exibido na UI da lixeira
    qint64  deletedAtMs = 0;
};

namespace TrashService {

QString trashRootPath();
QString trashedProjectsPath();

// Move a pasta inteira do projeto pra lixeira em vez de apagar. Registra a
// entrada em index.json. Retorna false (com *errorOut preenchido) se não
// conseguir mover nem copiar+remover.
bool trashProject(const QString& projectPath, QString* errorOut = nullptr);

QVector<TrashedProjectEntry> listTrashedProjects();

// Devolve o projeto pro lugar original (ou um caminho alternativo, se algo
// já ocupa o original) e some com a entrada da lixeira. Se restoredPathOut
// não for nullptr, recebe o caminho final restaurado.
bool restoreProject(const QString& id, QString* restoredPathOut = nullptr, QString* errorOut = nullptr);

// Apaga em definitivo — o único "excluir de verdade" que sobra no app pra
// projeto inteiro. Só deve ser chamado a partir da própria UI da lixeira.
bool purgeProject(const QString& id, QString* errorOut = nullptr);

} // namespace TrashService
