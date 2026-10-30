#pragma once
// Tags livres do projeto (29/10/2026, ideia do Pe): "guerra", "monarquia",
// "linhagem"... A pessoa põe a tag onde quiser (documento de gaveta, lugar e
// sistema do Criador de Mundos e as seções deles, memória do Pensário) e depois
// filtra por ela (o Explorar do mapa). Uma tag é só texto: não tem cadastro,
// existe enquanto alguma coisa usar. A grafia que vale é a primeira que apareceu
// ("Guerra" e "guerra" são a mesma tag).

#include <QString>
#include <QStringList>

#include <functional>

namespace ProjectTags {

// "  #Guerra   Santa " → "Guerra Santa" (sem #, espaços simples, no máximo 40 letras)
QString normalize(const QString& raw);
// normaliza cada uma e tira as repetidas (sem diferenciar maiúscula), na ordem
QStringList clean(const QStringList& tags);
bool has(const QStringList& tags, const QString& tag);   // sem diferenciar maiúscula

// As tags do projeto, das mais usadas pras menos (o MainWindow ensina a contar).
void setProvider(std::function<QStringList()> fn);
QStringList vocabulary();
// conta as listas e devolve as tags distintas, das mais usadas pras menos (empate: alfabética)
QStringList rank(const QList<QStringList>& lists);
// a grafia do projeto, se a tag já existe ("guerra" vira "Guerra" se assim nasceu)
QString canonical(const QString& tag, const QStringList& vocabulary);

} // namespace ProjectTags
