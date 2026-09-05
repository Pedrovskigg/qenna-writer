#pragma once

#include <QString>

// Trava central contra o app tratar uma pasta "de sistema" (Documentos,
// Área de Trabalho, pasta pessoal do usuário, Downloads, raiz de uma
// unidade, etc.) como se fosse a pasta de um projeto — nem pra abrir/
// registrar, nem pra excluir/mover.
//
// Existe porque até 2026-09-03 não havia NENHUMA validação: "Carregar
// pasta" deixava escolher literalmente qualquer diretório (o seletor até
// abre dentro de Documentos por padrão), e o antigo "Excluir projeto" fazia
// QDir::removeRecursively() direto no caminho escolhido. Um usuário
// selecionou a própria pasta Documentos sem perceber (ela virou "um
// projeto" com as subpastas do Qenna criadas dentro dela), excluiu esse
// "projeto" achando que era lixo de teste, e o app apagou a pasta
// Documentos inteira — não só os projetos do Qenna, TUDO que morava lá.
// Ver memória "qenna-trash-bin-decision" (fora do repo) pro relato completo.
namespace SystemFolderGuard {

// True se 'path' é uma pasta protegida. Se reasonOut não for nullptr, recebe
// uma descrição curta e traduzida do que a pasta é (para mostrar ao usuário
// no motivo da recusa).
bool isProtected(const QString& path, QString* reasonOut = nullptr);

} // namespace SystemFolderGuard
