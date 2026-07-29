#pragma once

#include <QString>

class ProjectModel;
class ElementsStore;
class DocCache;
struct DrawerItem;

// Resolve o conteúdo HTML "de verdade" de um item de gaveta (personagem,
// objeto, cenário) pra pré-visualização somente-leitura — ficha estruturada
// (sintetizada via ProjectModel::characterSheetToHtml) ou doc livre (cache/
// html inline/arquivo em disco). Extraído de RefMenuPanel::resolveDocHtml
// (que resolve isso e mais — capítulo, cena, nó do Construtor — mas só esta
// parte, a de item de gaveta, é reaproveitada fora do RefMenu).
namespace DocPreview {

// `includePhoto`: RefMenu embute a foto no topo da ficha (ele mesmo extrai a
// imagem de volta pra uma faixa lateral própria). Quem já mostra a foto em
// outro widget (ex. StatsPanel) passa false pra não duplicar/estourar a
// largura com uma <img> solta dentro do texto.
QString resolveDrawerItemHtml(const DrawerItem* item, ElementsStore* elements,
                               DocCache* cache, const QString& projectRoot,
                               bool includePhoto = true);

// Remove qualquer <img ...> do HTML (retrato embutido pela ficha OU imagem
// colada dentro do conteúdo de um campo/doc livre) — pra previews só-texto
// que não têm espaço/motivo pra renderizar imagem embutida.
QString stripImages(const QString& html);

// Remove cor de texto embutida inline (herdada do tema do editor no momento
// em que o campo/doc foi escrito, via toHtml()) — sem isso, o texto atropela
// a cor de tema do widget que mostra o preview (ex. StatsPanel), podendo
// ficar ilegível dependendo da combinação de temas. Preserva background-color
// (marcador de texto é formatação intencional do autor, não sobra de tema).
QString stripForegroundColors(const QString& html);

} // namespace DocPreview
