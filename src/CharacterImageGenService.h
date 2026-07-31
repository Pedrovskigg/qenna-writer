#pragma once

#include "AIClient.h" // AIChatMessage

#include <QImage>
#include <QObject>
#include <QString>
#include <QVector>

struct CharacterSheet;
class ImageGenClient;

// Presets de estilo oferecidos na geração de imagem — viram uma direção de
// arte no prompt engenhado (passo 1), nunca sobrepõem a cena pedida.
enum class ImageStylePreset {
    Default,
    Photorealistic,
    DigitalRealism,
    DigitalIllustration,
    Anime,
    Cartoon,
};

QString imageStylePresetLabel(ImageStylePreset preset);           // rótulo pra UI (combo)
QString imageStylePresetKey(ImageStylePreset preset);              // chave estável (schema da tool)
ImageStylePreset imageStylePresetFromKey(const QString& key);

// Orquestra o pipeline de 2 passos da geração de imagem de personagem:
// (1) chat completion de texto que ENGENHA o prompt de imagem a partir da
//     descrição livre do usuário + preset de estilo + (opcional) ficha do
//     personagem — o usuário nunca escreve o prompt final;
// (2) chamada real à API de imagem (GPT Image) com esse prompt.
// Instanciado de forma efêmera por quem for disparar uma geração (mesmo
// padrão já usado em AIChatPanel pras sub-chamadas de AIClient em
// handleReadDocumentTool/handleResummarizeDocumentTool) — mas, ao contrário
// dessas, pode viver por VÁRIAS rodadas (generate() + N refine()) enquanto
// o usuário pedir correções sobre a última geração, já que a conversa de
// engenharia de prompt precisa lembrar o que já foi gerado antes.
class CharacterImageGenService : public QObject {
    Q_OBJECT
public:
    explicit CharacterImageGenService(QObject* parent = nullptr);

    // Achata os campos da ficha (label: valor; blocos "text" convertidos de
    // HTML pra texto plano) num único texto — contexto de aparência pro
    // passo 1. Ficha vazia devolve QString() (sem seção de ficha no prompt).
    static QString sheetContextText(const CharacterSheet& sheet);

    // Início de uma conversa nova — characterContext vazio = geração livre,
    // sem personagem alvo. Zera qualquer conversa de correção anterior.
    // referenceImage (opcional, nula = sem referência): foto de referência
    // anexada pelo autor — a IA a DESCREVE em palavras (traços, roupas,
    // cores, pose, estilo) e incorpora isso no prompt engenhado. Não é
    // edição de imagem de verdade (a referência não vira pixel na saída,
    // só descrição textual) — GPT Image não recebe a foto em si.
    void generate(const QString& userDescription, ImageStylePreset style,
                  const QString& characterContext,
                  const QString& imageModel, const QString& imageQuality,
                  const QString& imageSize, const QImage& referenceImage = QImage());

    // Pede uma correção sobre a ÚLTIMA geração (chamar só depois de um
    // generate() bem-sucedido) — a IA ajusta o prompt levando em conta toda
    // a conversa até aqui (o prompt anterior + todas as correções já
    // aplicadas), em vez de partir do zero a cada rodada. Reaproveita
    // model/quality/size já definidos em generate().
    void refine(const QString& correctionText);

    // Pula a engenharia de prompt inteira — manda rawPrompt exatamente como
    // foi escrito, direto pra API de imagem. Pra quando o autor não quer
    // que a IA reescreva nada (prompt já pronto, escrito por ele mesmo ou
    // colado de outro lugar). Não inicia conversa de correção (refine() não
    // se aplica depois disso — não há prompt "engenhado pela IA" pra
    // ajustar; o autor edita o próprio texto e chama de novo).
    void generateFromRawPrompt(const QString& rawPrompt, const QString& imageModel,
                               const QString& imageQuality, const QString& imageSize);

signals:
    void promptEngineered(const QString& imagePrompt); // fim do passo 1, antes da chamada de imagem
    void imageReady(const QImage& image);                // fim do passo 2
    void errorOccurred(const QString& message);

private:
    void runPromptStep();
    void runImageStep(const QString& prompt);

    QVector<AIChatMessage> m_promptMessages; // conversa completa da engenharia de prompt, reaproveitada em refine()
    AIClient* m_promptClient = nullptr;
    ImageGenClient* m_imageClient = nullptr;

    QString m_imageModel;
    QString m_imageQuality;
    QString m_imageSize;
};
