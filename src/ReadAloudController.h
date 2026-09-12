#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

class QTextDocument;
class QTextToSpeech;
class QVoice;

// Motor de "ler em voz alta" — ferramenta de REVISÃO, não interface por voz.
// Ouvir o próprio texto é o jeito mais barato de pegar frase truncada,
// repetição e diálogo que não soa natural. (O TTS/STT da assistente Mira é
// outra coisa e está engavetado.)
//
// Fala em PEDAÇOS, não o documento inteiro de uma vez. Dois motivos:
//  - o realce precisa saber a posição no documento, e o offset que o Qt
//    devolve em sayingWord() é relativo ao texto entregue ao say(); guardando
//    o início de cada pedaço, a conta vira uma soma;
//  - fila de pedaços permite parar no fim de uma frase em vez de cortar o
//    sintetizador no meio.
class ReadAloudController : public QObject {
    Q_OBJECT
public:
    explicit ReadAloudController(QObject* parent = nullptr);

    // Disponível só se o sistema tiver algum motor de voz instalado. Em
    // Windows sempre tem (SAPI), mas não custa checar antes de oferecer a UI.
    bool isAvailable() const { return m_tts != nullptr; }

    // Vozes do idioma corrente do motor, na ordem em que o sistema devolve.
    QStringList voiceNames() const;
    int currentVoiceIndex() const;
    void setVoiceIndex(int index);

    // Idioma da leitura. Recebe o código do corretor ("pt_BR", "en_US") e
    // escolhe a voz instalada mais próxima; sem voz para o idioma, mantém a
    // do sistema em vez de emudecer.
    void setLanguageCode(const QString& code);
    QString languageCode() const { return m_languageCode; }

    // Escala do Qt: -1.0 (lento) a 1.0 (rápido), 0.0 = normal. A UI mostra
    // multiplicador ("1,2x") e converte — quem guarda em QSettings é ela.
    double rate() const { return m_rate; }
    void setRate(double rate);
    double volume() const { return m_volume; }
    void setVolume(double volume);

    bool isSpeaking() const;
    bool isPaused() const;

    // O motor sabe realçar palavra por palavra? Depende da voz instalada —
    // sem isso a UI realça o parágrafo inteiro, que ainda é útil.
    bool supportsWordProgress() const;

    // Lê do trecho pedido até o fim do documento. `fromPosition` é posição de
    // QTextDocument; a leitura começa no início da FRASE que contém essa
    // posição, não no meio dela.
    void speakDocument(const QTextDocument* doc, int fromPosition);
    // Lê só o intervalo pedido (usado quando há seleção).
    void speakRange(const QTextDocument* doc, int start, int end);

    void pause();
    void resume();
    void stop();

signals:
    // speaking = está lendo (mesmo pausado); paused = pausado de verdade.
    void stateChanged(bool speaking, bool paused);
    // Trecho a realçar, em posições de documento. len = 0 limpa o realce.
    void highlightRange(int start, int len);
    // Chegou ao fim da fila por conta própria (não por stop()).
    void finished();

private:
    struct Chunk {
        int docStart = 0;   // posição no QTextDocument onde o texto começa
        QString text;
    };

    void buildChunks(const QTextDocument* doc, int start, int end);
    void speakNext();
    void emitState();

    QTextToSpeech* m_tts = nullptr;
    QVector<Chunk> m_chunks;
    int m_index = -1;       // pedaço sendo lido
    bool m_stopping = false; // stop() pedido: ignora o Ready que vem depois
    // say() foi chamado mas o motor ainda não confirmou que começou. Sem isso,
    // o Ready ATRASADO de um stop() anterior (o Qt entrega o estado de forma
    // assíncrona) seria lido como "acabou o pedaço" e pularia o primeiro da
    // fila nova — some justamente a frase onde o autor mandou começar.
    bool m_waitingForStart = false;
    QString m_languageCode;
    double m_rate = 0.0;    // escala do Qt: -1.0 (lento) a 1.0 (rápido)
    double m_volume = 1.0;
};
