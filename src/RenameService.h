#pragma once

#include <QList>
#include <QString>
#include <QStringList>

class ProjectModel;
class ElementsStore;
class DialogueStore;
class MemoriesStore;
class MapPinsStore;
class ConstrutorStore;
class TerritorioStore;

// Renomeia um elemento (personagem, lugar, objeto) em todo o projeto de uma vez.
//
// O nome de um elemento vive em muito mais lugar do que o autor imagina, e até
// aqui só duas camadas eram atualizadas: Element::name e DrawerItem::title. O
// resto ficava dessincronizado em silêncio.
//
// As ocorrências se dividem em duas naturezas, e a diferença decide se a troca
// pode ser automática:
//
//  - Linked: existe um id do lado do nome, então a troca é certeza. É o caso
//    das @menções — gravadas como <a href="ref:<gaveta>:<itemId>">Nome</a>, em
//    que o texto visível é só rótulo do id que está no href.
//  - Loose: casamento por texto puro, sem id nenhum. É o nome escrito na prosa
//    ou num campo de ficha. Pode ser homônimo, pode ser parte de outra palavra,
//    pode ser intencional — precisa do olho do autor antes de aplicar.
//
// Camadas que guardam id (diálogos detectados, memórias, trilhas da Timeline,
// territórios de origem/local atual) não aparecem em Plan nenhum: resolvem o
// nome ao vivo e não precisam de nada.
class RenameService {
public:
    enum class Certainty {
        Linked,
        Loose,
    };

    enum class Layer {
        ChapterMention,   // @menção dentro de um capítulo
        ChapterProse,     // nome solto no texto do capítulo
        VariationMention, // idem, numa variação de cena não-ativa
        VariationProse,
        ItemMention,      // @menção dentro de um documento de gaveta
        ItemProse,
        SheetField,       // campo de ficha de personagem estruturada
    };

    // Um termo procurado e o que entra no lugar dele. O nome completo não é o
    // único: a prosa quase sempre chama o personagem só pelo primeiro nome, e
    // os apelidos cadastrados na ficha valem pelo mesmo motivo. Sem isso a
    // renomeação troca uns trechos e deixa outros, que foi o primeiro bug real
    // relatado.
    struct Term {
        QString find;
        QString replace;
        // Veio de um apelido cadastrado, não do nome que está sendo trocado.
        // Ambíguo por natureza: "Capitã" é apelido de verdade e deve ficar,
        // mas um nome antigo que virou apelido numa renomeação anterior (o
        // "manter como apelido" faz isso) precisa ser trocado. Como não dá pra
        // distinguir os dois pelo dado, vem desmarcado e o autor decide.
        bool fromAlias = false;
    };

    struct Occurrence {
        Layer layer;
        Certainty certainty = Certainty::Loose;
        QString where;        // rótulo legível pro preview ("Capítulo 3 — Cena 2")
        QString snippet;      // trecho ao redor, com o nome no meio
        QString matched;      // texto que casou (pode ser o 1º nome, não o completo)
        QString replacement;  // o que entra no lugar dele
        bool fromAlias = false; // veio de apelido cadastrado — entra desmarcado
        bool selected = true;

        // Endereço interno — não é exibido, só usado no apply().
        QString filePath;   // caminho absoluto do arquivo que contém o texto
        QString relFile;    // caminho relativo (capítulos passam por writeChapter)
        QString ownerId;    // itemId da gaveta, quando a origem é item/ficha
        QString fieldId;    // SheetField::id, quando a origem é ficha
        int position = -1;  // posição no HTML de origem
        int length = 0;
    };

    // Resultado de uma varredura: o que seria trocado se o autor confirmar.
    // As ocorrências vêm com selected=true nas Linked e nas Loose — o preview é
    // que deixa o autor desmarcar; nada é aplicado sem passar por apply().
    struct Plan {
        QString elementId;
        QString oldName;
        QString newName;
        QList<Term> terms;      // o que foi procurado (nome inteiro, 1º nome)
        QStringList aliases;    // apelidos do elemento — não são trocados, só avisados
        QList<Occurrence> occurrences;

        // Guarda o nome antigo como apelido do elemento. Ligado por padrão: a
        // detecção de presença e o detector de diálogos casam por name+aliases,
        // então sem isso tudo que já foi detectado pelo nome velho se perde.
        bool keepOldAsAlias = true;

        int selectedCount() const;
        int countOf(Certainty c) const;
        bool isEmpty() const { return occurrences.isEmpty(); }
    };

    RenameService(ProjectModel* model, ElementsStore* elements, const QString& projectRoot);

    // Sidecars que guardam SNAPSHOT de texto do manuscrito (a fala detectada, o
    // trecho da memória). Eles não referenciam o texto, copiam — então
    // envelhecem quando o manuscrito é renomeado, e no caso das falas isso
    // quebra o rescan, que identifica cada uma pelo hash do próprio texto.
    // Opcionais: sem eles a renomeação funciona, só não atualiza as cópias.
    void setDialogueStore(DialogueStore* s) { m_dialogueStore = s; }
    void setMemoriesStore(MemoriesStore* s) { m_memoriesStore = s; }
    // Pin do mapa: guarda o id do item e o nome só como cache de exibição.
    void setMapPinsStore(MapPinsStore* s) { m_mapPinsStore = s; }
    // Construtor e Criador de Mundos guardam SNAPSHOT do trecho citado, igual
    // aos diálogos e às memórias.
    void setConstrutorStore(ConstrutorStore* s) { m_construtorStore = s; }
    void setTerritorioStore(TerritorioStore* s) { m_territorioStore = s; }

    // Varre o projeto sem alterar nada. oldName é explícito (e não lido do
    // ElementsStore) porque o fluxo de edição do item já gravou o nome novo
    // antes de chegar aqui — a essa altura o store não sabe mais o nome antigo.
    Plan scan(const QString& elementId, const QString& oldName, const QString& newName) const;

    // Aplica o plano: grava os arquivos das ocorrências marcadas e atualiza o
    // nome canônico (Element::name, DrawerItem::title e, se pedido, aliases).
    // Não salva o projeto nem invalida caches de detecção — quem chama faz isso,
    // porque só a MainWindow sabe o que está aberto no editor.
    bool apply(const Plan& plan, QString* error = nullptr);

    // Termos a procurar para um par (nome antigo → novo): o nome inteiro, o
    // primeiro nome quando o nome tem mais de uma palavra, e cada apelido.
    static QList<Term> termsFor(const QString& oldName, const QString& newName,
                                const QStringList& aliases);

    // Ocorrências dentro de um HTML. Exposto porque o preview precisa rodar
    // sobre o documento aberto (que pode ter edição não salva) em vez do
    // arquivo em disco.
    static QList<Occurrence> scanHtml(const QString& html, const QList<Term>& terms,
                                      const QString& itemIdForMentions,
                                      const QString& mentionReplacement,
                                      Layer mentionLayer, Layer proseLayer);

    // Reescreve um HTML aplicando as ocorrências marcadas. As posições são do
    // HTML original, então a aplicação anda de trás pra frente.
    static QString applyToHtml(const QString& html, const QList<Occurrence>& occurrences);

private:
    void scanChapters(Plan& plan) const;
    void scanDrawerItems(Plan& plan) const;
    QString itemIdForElement(const QString& elementId) const;
    // Camadas que guardam o nome como CACHE ao lado de um id: a troca é certa e
    // não passa pelo preview. Exceção deliberada é o título do card da Lousa,
    // que o autor pode ter editado à mão — ver o .cpp.
    void applyLinkedCaches(const Plan& plan);

    ProjectModel* m_model = nullptr;
    ElementsStore* m_elements = nullptr;
    DialogueStore* m_dialogueStore = nullptr;
    MemoriesStore* m_memoriesStore = nullptr;
    MapPinsStore* m_mapPinsStore = nullptr;
    ConstrutorStore* m_construtorStore = nullptr;
    TerritorioStore* m_territorioStore = nullptr;
    QString m_root;
};
