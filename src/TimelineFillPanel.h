#pragma once
// Painel "Preencher" da Timeline — o antigo Gerador de Timeline, que morava
// num diálogo em Configurações. Um capítulo por vez, no lugar do painel
// lateral: mostra como o capítulo começa (pra lembrar quando ele acontece),
// sugere o marcador a partir da primeira frase, e pede "quando se passa" e o
// resumo. "Salvar e próximo" anda a fila (sem data / sem resumo / todos).
//
// Capítulo com cenas é preenchido inteiro (as cenas herdam); "separar cenas"
// dá um campo pra cada cena, pro caso de cenas em momentos diferentes.
// Capítulo cujas cenas já têm tempo/resumo próprio abre separado.
//
// Como as views irmãs, não mexe no modelo: cada campo editado sobe por sinal
// e o TimelinePanel grava. Os campos gravam ao sair deles.

#include "TimelineTracksTypes.h"
#include "TimelineFillAssist.h"

#include <QPointer>
#include <QSet>
#include <QWidget>
#include <functional>

class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QTextEdit;
class QToolButton;
class QVBoxLayout;
class AIClient;

namespace FillDetail { class Progress; }

class TimelineFillPanel : public QWidget {
    Q_OBJECT
public:
    static constexpr int kWidth = 390;

    explicit TimelineFillPanel(QWidget* parent = nullptr);

    // "ch:<id>" / "sc:<id>" → texto; "full:ch:<id>" → o capítulo inteiro (pro resumo pela IA)
    void setDocTextResolver(std::function<QString(const QString&)> r) { m_resolver = std::move(r); }

    // Dados novos (depois de cada gravação): atualiza sem mexer no campo em edição.
    void setData(const Tracks::Data& d);
    // Monta a fila e abre nela. chapterId vazio = primeiro da fila.
    void start(const QString& chapterId = QString());
    // Vai direto pra um capítulo (clique na Timeline). Fora da fila atual, passa pra "Todos".
    void jumpTo(const QString& chapterId);
    void commitPending();

signals:
    void edited(const QString& colKey, bool chapterLevel, bool summary, const QString& text);
    void closeRequested();
    void currentChanged(int firstCol, int span);   // -1 = nenhum (fila concluída)

protected:
    void paintEvent(QPaintEvent* e) override;
    bool eventFilter(QObject* o, QEvent* e) override;

private:
    friend class FillDetail::Progress;
    enum Filter { NoDate = 0, NoSummary = 1, All = 2 };

    // um par de campos (marcador + resumo) ligado a um capítulo ou a uma cena
    struct Fields {
        int        col = -1;            // coluna (cena) ou primeira coluna do capítulo
        bool       chapterLevel = true;
        QLineEdit* fld = nullptr;
        QLabel*    lei = nullptr;
        QTextEdit* txa = nullptr;
        QString    boundMarker, boundSummary;
    };

    // capítulo = colunas [c0, c0+span)
    struct Chap { QString id; int c0 = -1; int span = 0; bool hasScenes = false; };
    QList<Chap> chapters() const;
    Chap chapterById(const QString& id) const;
    bool chapterMissing(const Chap& c, Filter f) const;
    bool isSplit(const Chap& c) const;
    bool canJoin(const Chap& c) const;

    QString effMarker(int col) const;
    QString effSummary(int col) const;
    bool    hollow(int col) const;

    void buildQueue(const QString& focusChapter);
    void showCurrent();              // (re)constrói o corpo pro capítulo atual
    void refreshDerived();           // leituras, sugestão, contadores — sem recriar campos
    void refreshReading(Fields& f);
    void goNext();
    void goPrev();
    void setFilter(Filter f);
    void commit(Fields& f);
    void askAiSummary();
    QString signature() const;       // muda quando o corpo precisa ser recriado
    Fields* fieldsOf(QObject* o);

    Tracks::Data m_data;
    std::function<QString(const QString&)> m_resolver;

    Filter        m_filter = NoDate;
    QStringList   m_queue;           // capítulos da fila (ficam nela até fechar, mesmo preenchidos)
    int           m_idx = 0;
    QSet<QString> m_splitChapters;   // "separar cenas" escolhido nesta sessão
    QString       m_builtSig;

    // corpo do capítulo atual
    QList<Fields> m_fields;
    QWidget*      m_body = nullptr;
    QLabel*       m_excerpt = nullptr;
    QWidget*      m_sugBox = nullptr;
    QLabel*       m_sugMarker = nullptr;
    QLabel*       m_sugWhy = nullptr;
    QPushButton*  m_aiBtn = nullptr;
    QPushButton*  m_ditto = nullptr;
    QPushButton*  m_plus = nullptr;
    QString       m_openingText;     // começo do capítulo (pra sugestão e trecho)
    FillAssist::Suggestion m_sug;
    QPointer<AIClient> m_ai;

    // casca
    QToolButton*  m_segBtn[3] = {};
    FillDetail::Progress* m_progress = nullptr;
    QStackedWidget* m_pages = nullptr;
    QScrollArea*  m_scroll = nullptr;
    QWidget*      m_donePage = nullptr;
    QLabel*       m_doneText = nullptr;
    QPushButton*  m_prevBtn = nullptr;
    QPushButton*  m_nextBtn = nullptr;
    QLabel*       m_countLbl = nullptr;
    QWidget*      m_foot = nullptr;
    QToolButton*  m_closeBtn = nullptr;

    void applyTheme();
};
