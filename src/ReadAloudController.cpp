#include "ReadAloudController.h"

#include "CrashLogger.h"

#include <QLocale>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextToSpeech>
#include <QVoice>

namespace {

// Fim de frase. O ':' entra porque em prosa ele costuma abrir fala ou lista,
// e a pausa soa natural; a reticência já vem tratada como caractere único.
bool isSentenceEnd(QChar c)
{
    return c == QLatin1Char('.') || c == QLatin1Char('!') || c == QLatin1Char('?')
        || c == QLatin1Char(':') || c == QChar(0x2026);
}

// Um bloco muito longo sem pontuação (acontece em rascunho) viraria um pedaço
// só, e aí parar a leitura demoraria uma eternidade. Corta no espaço seguinte.
constexpr int kMaxChunkChars = 400;

} // namespace

ReadAloudController::ReadAloudController(QObject* parent)
    : QObject(parent)
{
    // Sem motor de voz instalado o construtor ainda funciona, mas o estado
    // fica Error — tratamos como indisponível em vez de deixar a UI prometer
    // algo que não vai sair.
    auto* tts = new QTextToSpeech(this);
    if (tts->state() == QTextToSpeech::Error) {
        delete tts;
        return;
    }
    m_tts = tts;
    m_tts->setVolume(m_volume);
    m_tts->setRate(m_rate);

    connect(m_tts, &QTextToSpeech::stateChanged, this,
            [this](QTextToSpeech::State state) {
        if (state == QTextToSpeech::Speaking || state == QTextToSpeech::Synthesizing) {
            // O motor confirmou que o pedaço atual começou: a partir daqui, um
            // Ready significa mesmo "terminou".
            m_waitingForStart = false;
        } else if (state == QTextToSpeech::Ready) {
            // Fim de um pedaço: emenda o próximo. O stop() do usuário também
            // gera Ready, por isso as travas — senão a fila recomeçaria
            // sozinha, ou pularia o pedaço que acabou de ser enfileirado.
            if (!m_stopping && !m_waitingForStart && m_index >= 0) {
                speakNext();
                return;
            }
        }
        if (state == QTextToSpeech::Error) {
            CrashLogger::log(QStringLiteral("readAloud ERRO do motor: %1")
                             .arg(m_tts->errorString()));
            m_index = -1;
            m_chunks.clear();
            emit highlightRange(0, 0);
        }
        emitState();
    });

    connect(m_tts, &QTextToSpeech::sayingWord, this,
            [this](const QString&, qsizetype, qsizetype start, qsizetype length) {
        if (m_index < 0 || m_index >= m_chunks.size()) return;
        const Chunk& c = m_chunks.at(m_index);
        emit highlightRange(c.docStart + int(start), int(length));
    });
}

QStringList ReadAloudController::voiceNames() const
{
    QStringList out;
    if (!m_tts) return out;
    const QList<QVoice> voices = m_tts->availableVoices();
    out.reserve(voices.size());
    for (const QVoice& v : voices) out << v.name();
    return out;
}

int ReadAloudController::currentVoiceIndex() const
{
    if (!m_tts) return -1;
    const QList<QVoice> voices = m_tts->availableVoices();
    const QVoice cur = m_tts->voice();
    for (int i = 0; i < voices.size(); ++i) {
        if (voices.at(i).name() == cur.name()) return i;
    }
    return -1;
}

void ReadAloudController::setVoiceIndex(int index)
{
    if (!m_tts) return;
    const QList<QVoice> voices = m_tts->availableVoices();
    if (index < 0 || index >= voices.size()) return;
    m_tts->setVoice(voices.at(index));
}

void ReadAloudController::setLanguageCode(const QString& code)
{
    if (!m_tts || code.isEmpty()) return;
    m_languageCode = code;

    QLocale wanted(code);
    // Casar por idioma E país quando possível: pt_BR e pt_PT soam bem
    // diferentes, e ouvir a própria prosa com o sotaque errado atrapalha mais
    // do que ajuda na revisão.
    const QList<QLocale> locales = m_tts->availableLocales();
    QLocale best;
    bool exact = false, sameLanguage = false;
    for (const QLocale& l : locales) {
        if (l.language() == wanted.language() && l.territory() == wanted.territory()) {
            best = l; exact = true; break;
        }
        if (!sameLanguage && l.language() == wanted.language()) {
            best = l; sameLanguage = true;
        }
    }
    if (!exact && !sameLanguage) return;  // sem voz do idioma: mantém a atual
    m_tts->setLocale(best);
}

void ReadAloudController::setRate(double rate)
{
    m_rate = qBound(-1.0, rate, 1.0);
    if (m_tts) m_tts->setRate(m_rate);
}

void ReadAloudController::setVolume(double volume)
{
    m_volume = qBound(0.0, volume, 1.0);
    if (m_tts) m_tts->setVolume(m_volume);
}

bool ReadAloudController::isSpeaking() const
{
    if (!m_tts) return false;
    const QTextToSpeech::State s = m_tts->state();
    return s == QTextToSpeech::Speaking || s == QTextToSpeech::Synthesizing
        || s == QTextToSpeech::Paused;
}

bool ReadAloudController::isPaused() const
{
    return m_tts && m_tts->state() == QTextToSpeech::Paused;
}

bool ReadAloudController::supportsWordProgress() const
{
    if (!m_tts) return false;
    return m_tts->engineCapabilities().testFlag(QTextToSpeech::Capability::WordByWordProgress);
}

void ReadAloudController::buildChunks(const QTextDocument* doc, int start, int end)
{
    m_chunks.clear();
    if (!doc) return;

    for (QTextBlock blk = doc->findBlock(start); blk.isValid(); blk = blk.next()) {
        const int blockStart = blk.position();
        if (blockStart >= end) break;

        QString text = blk.text();
        if (text.isEmpty()) continue;
        // Imagem inline vira U+FFFC. Troca por espaço em vez de remover: um
        // caractere a menos aqui desalinharia todo o realce do parágrafo.
        text.replace(QChar(0xFFFC), QLatin1Char(' '));

        // Recorta o bloco ao intervalo pedido (importa no primeiro e no
        // último bloco de uma seleção).
        const int from = qMax(0, start - blockStart);
        const int to = qMin(text.size(), end - blockStart);
        if (from >= to) continue;

        int i = from;
        while (i < to) {
            int j = i;
            int lastSpace = -1;
            while (j < to) {
                if (isSentenceEnd(text.at(j))) {
                    // Consome a pontuação e o que vier colado (aspas, fecha
                    // parêntese) pra não começar a próxima frase com lixo.
                    ++j;
                    while (j < to && !text.at(j).isSpace() && !text.at(j).isLetterOrNumber())
                        ++j;
                    break;
                }
                if (text.at(j).isSpace()) lastSpace = j;
                ++j;
                if (j - i >= kMaxChunkChars) {
                    if (lastSpace > i) j = lastSpace;
                    break;
                }
            }

            const QString piece = text.mid(i, j - i);
            // Offset do primeiro caractere que não é espaço: o realce nunca
            // deve começar num espaço em branco.
            int lead = 0;
            while (lead < piece.size() && piece.at(lead).isSpace()) ++lead;
            const QString trimmed = piece.mid(lead).trimmed();
            if (!trimmed.isEmpty()) {
                Chunk c;
                c.docStart = blockStart + i + lead;
                c.text = trimmed;
                m_chunks.append(c);
            }
            i = j;
        }
    }
}

void ReadAloudController::speakDocument(const QTextDocument* doc, int fromPosition)
{
    if (!doc) return;

    // Recua até o começo da frase: clicar no meio de um parágrafo e ouvir a
    // leitura entrar por "…ela disse, e saiu." não ajuda ninguém a revisar.
    int start = qMax(0, fromPosition);
    const QTextBlock blk = doc->findBlock(start);
    if (blk.isValid()) {
        const QString text = blk.text();
        const int offset = qBound(0, start - blk.position(), text.size());
        int begin = 0;
        for (int k = 0; k < offset; ++k) {
            if (isSentenceEnd(text.at(k))) begin = k + 1;
        }
        while (begin < text.size() && text.at(begin).isSpace()) ++begin;
        // Com o cursor logo depois de um ponto, `begin` já aponta pra frase
        // seguinte — que é justamente onde se quer continuar.
        start = blk.position() + begin;
    }

    speakRange(doc, start, doc->characterCount());
}

void ReadAloudController::speakRange(const QTextDocument* doc, int start, int end)
{
    if (!m_tts || !doc) return;
    stop();
    buildChunks(doc, qMax(0, start), qMax(0, end));
    if (m_chunks.isEmpty()) {
        emit finished();
        return;
    }
    CrashLogger::log(QStringLiteral("readAloud speakRange start=%1 end=%2 pedacos=%3 voz=%4")
                     .arg(start).arg(end).arg(m_chunks.size())
                     .arg(m_tts->voice().name()));
    m_stopping = false;
    m_index = -1;
    speakNext();
}

void ReadAloudController::speakNext()
{
    if (!m_tts) return;
    ++m_index;
    if (m_index >= m_chunks.size()) {
        m_index = -1;
        m_waitingForStart = false;
        m_chunks.clear();
        emit highlightRange(0, 0);
        emitState();
        emit finished();
        return;
    }
    const Chunk& c = m_chunks.at(m_index);
    // Um pedaço é uma frase e leva segundos pra ser falado, então gravar cada
    // um não pesa — e é o rastro que diz ONDE a leitura estava se algo cair.
    CrashLogger::log(QStringLiteral("readAloud pedaco %1/%2 docStart=%3 chars=%4")
                     .arg(m_index + 1).arg(m_chunks.size())
                     .arg(c.docStart).arg(c.text.size()));
    // Sem realce palavra a palavra, o parágrafo/frase corrente já orienta o
    // olho — melhor isso do que não realçar nada.
    if (!supportsWordProgress())
        emit highlightRange(c.docStart, c.text.size());
    m_waitingForStart = true;
    m_tts->say(c.text);
    emitState();
}

void ReadAloudController::pause()
{
    if (!m_tts || !isSpeaking()) return;
    CrashLogger::log("readAloud pause");
    // Pausa no fim da palavra: no meio dela o retorno sai com um estalo.
    m_tts->pause(QTextToSpeech::BoundaryHint::Word);
    emitState();
}

void ReadAloudController::resume()
{
    if (!m_tts) return;
    CrashLogger::log("readAloud resume");
    m_tts->resume();
    emitState();
}

void ReadAloudController::stop()
{
    if (!m_tts) return;
    CrashLogger::log(QStringLiteral("readAloud stop (estava no pedaco %1 de %2)")
                     .arg(m_index + 1).arg(m_chunks.size()));
    m_stopping = true;
    m_waitingForStart = false;
    m_index = -1;
    m_chunks.clear();
    m_tts->stop(QTextToSpeech::BoundaryHint::Immediate);
    emit highlightRange(0, 0);
    emitState();
}

void ReadAloudController::emitState()
{
    emit stateChanged(isSpeaking(), isPaused());
}
