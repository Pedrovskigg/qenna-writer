#pragma once

#include <QString>

// Arquivo GLOBAL (não por-projeto, mesmo espírito de MiraStyleStore) que
// acumula notas sobre o AUTOR — preferências criativas, jeito de trabalhar,
// ideias soltas ainda sem projeto, vínculo/piadas internas — tudo que a Mira
// aprende sobre a pessoa e que continua valendo em qualquer projeto, aberto
// ou futuro. Diferente de memoria_mira.md (por-projeto, fatos da HISTÓRIA),
// este arquivo é sobre quem escreve, não sobre o que está sendo escrito.
//
// Fica em QStandardPaths::AppDataLocation, mesma pasta-base do
// mira_style.json e do crash.log. Formato markdown simples, append-only —
// mesmo formato de linha que memoria_mira.md já usa (ver
// AIChatPanel::handleSaveProjectNoteTool), sem JSON nem consolidação: é só
// texto injetado direto no system prompt.
//
// Namespace de funções livres, sem estado em memória — lê do disco toda vez
// que precisa (mesmo espírito de loadProjectMemoryFile() em AIChatPanel).
namespace MiraUserMemoryStore {

QString filePath();

// Conteúdo markdown inteiro do arquivo, "" se ainda não existir.
QString load();

// Quantas notas já foram salvas (conta linhas "- **[...", mesmo formato de
// appendEntry) — usado só pra exibir um número na UI (chip "N notas sobre
// você"), não pra lógica de negócio.
int entryCount();

// Anexa uma nova nota, no mesmo formato de linha usado por
// memoria_mira.md: "- **[Label]** emoji **Título** — conteúdo _(timestamp)_".
void appendEntry(const QString& category, const QString& status,
                 const QString& title, const QString& content);

} // namespace MiraUserMemoryStore
