#pragma once

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QPixmap>
#include <QPoint>
#include <QString>
#include <QVector>

#include <functional>

class QWidget;
struct WordCounterSettings;

// Um projeto da Biblioteca do menu principal, já lido do disco e pronto pra
// qualquer vista. Quem monta é o MainMenuDialog (lê o índice uma vez só).
struct LibraryEntry {
    QString path;
    QString name;
    QString author;
    QString genres;
    QString synopsis;
    QPixmap cover;             // capa efetiva; gerada quando o projeto não tem uma
    bool hasOwnCover = false;  // false = a capa acima é a gerada (nome sobre gradiente)
    int totalWords = -1;       // -1 = ainda não cacheado no índice
    int manuscriptCount = 0;
    int chapterCount = 0;
    QDateTime lastTouched;     // último "Onde parei" ou último save, o que vier depois
    QString resumeWhere;       // "cap. 7 · O que a aranha pediu"; vazio = sem Onde parei
    QString resumeSentence;    // última frase antes do cursor
    int finalChapters = 0;     // capítulos com status Final
    int statusChapters = 0;    // capítulos com algum status; 0 = ninguém usa status
    bool autoOpen = false;

    // Quanto do livro está pronto (capítulos Final / capítulos). -1 quando o
    // projeto não usa status: aí não tem barra, em vez de uma barra mentindo 0%.
    double progress() const
    {
        if (statusChapters <= 0 || chapterCount <= 0) return -1.0;
        return double(finalChapters) / chapterCount;
    }
};

// O dia de escrita do projeto em foco (ou da meta unificada, quando ligada):
// o que o contador de palavras já sabe, relido do índice sem abrir o projeto.
struct WritingDay {
    bool valid = false;
    bool timeGoal = false;     // meta em minutos em vez de palavras
    int done = 0;              // palavras (ou minutos) na janela de hoje
    int target = 0;
    int streak = 0;
    QVector<int> week;         // 7 valores; o último é hoje
    QVector<QDate> weekDays;
    QString scopeName;         // projeto dono da meta; vazio = meta unificada

    static WritingDay fromSettings(const WordCounterSettings& s, const QString& scopeName);
};

struct LibraryReminder {
    QString text;
    QString project;
    qint64 dueAt = 0;
};

// O que as vistas podem pedir ao menu. Todas recebem o caminho do projeto.
struct LibraryHooks {
    std::function<void(const QString&)> open;
    std::function<void(const QString&)> resume;      // abre e volta pro Onde parei
    std::function<void(const QString&)> details;     // diálogo Editar projeto
    std::function<void(const QString&, const QPoint&)> menu; // botão direito
    std::function<void()> newProject;
    std::function<void()> newIdea;
    std::function<void()> loadProject;
    std::function<void(const QString&)> language;    // troca de idioma (reinicia)
};

// As vistas da Biblioteca. Cada uma devolve um widget novo, montado do zero;
// o menu joga fora o anterior a cada troca.
class LibraryViews {
    Q_DECLARE_TR_FUNCTIONS(LibraryViews)
public:
    static QWidget* continueView(const QVector<LibraryEntry>& entries, const WritingDay& day,
                                 const LibraryHooks& hooks, QWidget* parent);
    static QWidget* vitrineView(const QVector<LibraryEntry>& entries,
                                const LibraryHooks& hooks, QWidget* parent);
    static QWidget* cinemaView(const QVector<LibraryEntry>& entries,
                               const LibraryHooks& hooks, QWidget* parent);
    static QWidget* dayView(const QVector<LibraryEntry>& entries, const WritingDay& day,
                            const QVector<LibraryReminder>& reminders,
                            const LibraryHooks& hooks, QWidget* parent);
    static QWidget* shelfView(const QVector<LibraryEntry>& entries,
                              const LibraryHooks& hooks, QWidget* parent);
    // Primeira vez: ainda não há projeto nenhum.
    static QWidget* welcomeView(const LibraryHooks& hooks, QWidget* parent);

    // "agora há pouco", "há 2 horas", "ontem", "há 3 meses"…
    static QString relativeWhen(const QDateTime& when);
    static QString wordsText(int words);
};
