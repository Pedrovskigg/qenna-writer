#pragma once

#include <QJsonArray>
#include <QString>

// Arquivo GLOBAL (não por-projeto, diferente de resumo_projeto.md/
// memoria_mira.md) que acumula exemplos de resposta da Mira marcados com
// like/dislike pelo autor, e consolida periodicamente um resumo de estilo
// — sem retreinar nada, é puro contexto injetado no system prompt via
// buildPromptFragment(). Fica em QStandardPaths::AppDataLocation, mesma
// pasta-base do crash.log (ver CrashLogger.cpp).
//
// Namespace de funções livres, sem estado em memória: lê do disco toda vez
// que precisa (mesmo espírito de loadProjectSummaryFile() em AIChatPanel),
// já que a frequência de chamada (uma vez por like/dislike, uma vez por
// buildSystemPrompt) não justifica cache.
namespace MiraStyleStore {

struct Data {
    QJsonArray positiveExamples;   // cada item: {"text":..., "timestamp":...}
    QJsonArray negativeExamples;
    QString positiveSummary;       // vazio até a 1ª consolidação daquele lado
    QString negativeSummary;
};

QString filePath();

Data load();
void save(const Data& data);

// Chamado pelo clique de like/dislike numa bolha de resposta. Salva o
// exemplo imediatamente (antes de checar o teto — o exemplo nunca se perde
// mesmo que a consolidação abaixo falhe) e dispara a consolidação daquele
// lado se o array correspondente bateu o teto (positiveExamples.size()/
// negativeExamples.size() SÃO o contador — sem campo redundante).
void recordFeedback(bool positive, const QString& fullResponseText);

// Usado nos dois buildSystemPrompt() (AIChatPanel e AISelectionChat).
// Retorna string vazia se nunca houve consolidação em nenhum lado ainda
// (sem custo de token quando não há feedback acumulado).
QString buildPromptFragment();

} // namespace MiraStyleStore
