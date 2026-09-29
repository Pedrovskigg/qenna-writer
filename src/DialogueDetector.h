#pragma once

#include "DialogueLang.h"
#include "ElementsStore.h"

#include <QHash>
#include <QPair>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

class QTextDocument;

// Quanto o motor confia no locutor de uma fala.
//  Certain  — o nome está na tag ("— Não. — Klara disse.") ou no bloco de
//             personagem do roteiro. Na auditoria: 64 de 64 certas.
//  Probable — deduzido pela conversa (alternância, quem foi citado, pronome
//             + gênero). ~70% de acerto: a tela pede confirmação.
//  Extra    — tem locutor, mas fora do elenco ("O porteiro chamou",
//             "Éder atropelou"). extraLabel guarda como o texto o chama.
//  None     — sem pista nenhuma.
enum class DialogueConfidence { None, Certain, Probable, Extra };

struct DetectedDialogueLine {
    QString text;          // parágrafo inteiro (é o que o editor procura pra abrir)
    QString speech;        // só a fala, sem a tag — base de diálogo x narração
    QString characterId;   // vazio = sem locutor do elenco
    DialogueConfidence confidence = DialogueConfidence::None;
    QString extraLabel;    // figurante: "o porteiro", "Éder"
};

// Motor de atribuição de falas. Lê a CENA inteira, não uma linha solta:
//  1. corta cada parágrafo em trechos de fala e de narração (travessão,
//     meia-risca, hífen, aspas retas/curvas, « », ‘ ’, fala depois de ":");
//  2. procura o sujeito da tag (nome no começo da primeira oração, sem
//     preposição antes; pronome; figurante; 1ª pessoa do narrador);
//  3. sem sujeito nomeado, deduz pela conversa: quem falou antes, quem foi
//     citado, quem foi chamado pelo nome DENTRO da fala (esse não é quem fala).
// Protótipo e banco de testes: C:\mira-writing\concepts\harness\dialogos.
class DialogueDetector {
public:
    struct Token {
        QRegularExpression re;
        QString id;
    };
    struct Cast {
        QVector<Token> tokens;
        QSet<QString> ids;
        QHash<QString, QString> nameById;
        // Nome/apelido em minúsculas → id (só os sem ambiguidade): deixa do roteiro.
        QHash<QString, QString> idByLowerName;
        bool isEmpty() const { return ids.isEmpty(); }
    };
    // Voto de gênero por personagem: (feminino, masculino).
    using GenderVotes = QHash<QString, QPair<double, double>>;

    // Elenco: nome completo, primeiro nome (só se único) e apelidos. Nome
    // que começa com maiúscula só bate com maiúscula ("Rosa" não é "rosa").
    static Cast buildCast(const QList<Element>& elements);

    // Uma cena (ou capítulo sem cenas), parágrafos em ordem. A memória da
    // conversa zera entre chamadas — cena nova, conversa nova.
    // narratorId: POV do capítulo ou narrador global; vazio = sem narrador.
    // gender: 'f'/'m' por personagem (ver genderFromVotes).
    static QVector<DetectedDialogueLine> scanScene(const QStringList& paragraphs,
                                                   const Cast& cast,
                                                   const DialogueLang& lang,
                                                   const QString& narratorId,
                                                   const QHash<QString, QChar>& gender);

    // Roteiro: o locutor é o bloco Personagem acima do bloco Diálogo.
    static QVector<DetectedDialogueLine> scanScreenplay(const QTextDocument& doc, const Cast& cast);

    // Gênero pelo texto: artigo antes do nome ("a Klara") e pronome logo
    // depois. Soma em `votes`; genderFromVotes decide com folga de 1,5x.
    static void addGenderVotes(const QString& text, const Cast& cast, const DialogueLang& lang,
                               GenderVotes& votes);
    static QHash<QString, QChar> genderFromVotes(const GenderVotes& votes);

    // Parágrafos de um texto puro (QTextDocument::toPlainText separa blocos
    // com '\n').
    static QStringList paragraphsOf(const QString& plainText);

    // Migração do dialogs.json antigo: o que o motor ANTERIOR (até a 0.18)
    // responderia pra essa linha — nomes no "texto de atribuição" (tudo
    // depois do 2º travessão, ou depois da aspa de fechamento), sem
    // distinção de maiúscula, e um só personagem = dele. A busca por
    // proximidade daquele motor estava morta (regex grande demais), então
    // não entra. Se a atribuição salva bate com isso, ela foi automática;
    // se não bate, foi corrigida à mão.
    static QString legacyAttribution(const QString& text, const Cast& cast);
};
