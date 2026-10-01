#pragma once

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QWidget>

struct Element;
class QToolButton;
class QLabel;

// Breakdown do roteiro (o relatório de produção): quem fala quanto e em quais
// cenas, cada cena com local, INT/EXT, dia/noite e tamanho em oitavos de
// página, e quantas cenas cada local tem. Fica no painel de Estatísticas,
// só em projeto de roteiro.
namespace ScreenplayBreakdownData {

struct SceneRow {
    int number = 0;
    QString heading;
    QString location;
    bool interior = false;
    bool exterior = false;
    bool night = false;
    QStringList speakers;   // nomes de exibição, na ordem em que falam
    QStringList speakerKeys; // personagem (ficha) de cada nome acima — um nome por pessoa
    qreal pages = 0;        // tamanho, em páginas do formato padrão
    QString manuscriptId;
    QString chapterId;
    int sceneIndex = 0;     // índice da cena no capítulo (pra abrir)
    bool chapterHasScenes = false;
};

struct CastRow {
    QString name;           // nome do personagem (ou a deixa, se figurante)
    QString elementId;      // vazio = figurante
    QString image;          // foto (data URL), se houver
    int lines = 0;
    QSet<int> scenes;       // números das cenas
};

struct LocationRow {
    QString location;
    int scenes = 0, day = 0, night = 0;
};

struct Data {
    QList<SceneRow> scenes;
    QList<CastRow> cast;    // mais falas primeiro
    QList<LocationRow> locations;
    int totalLines = 0;
    qreal totalPages = 0;
};

struct ChapterInput {
    QString manuscriptId;
    QString chapterId;
    QString html;
};

Data compute(const QList<ChapterInput>& chapters, const QList<Element>& elements);

// "1 5/8", "3/8", "1": oitavos de página, como a produção mede.
QString eighths(qreal pages);

}

class ScreenplayBreakdown : public QWidget {
    Q_OBJECT
public:
    explicit ScreenplayBreakdown(QWidget* parent = nullptr);
    void setData(const ScreenplayBreakdownData::Data& data);

signals:
    void sceneActivated(QString manuscriptId, QString chapterId, int sceneIndex, bool chapterHasScenes);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    enum class View { Cast, Scenes, Locations };
    void setView(View v);
    void relayout();
    int contentHeight() const;

    ScreenplayBreakdownData::Data m_data;
    View m_view = View::Scenes;
    QToolButton* m_btnCast = nullptr;
    QToolButton* m_btnScenes = nullptr;
    QToolButton* m_btnLocations = nullptr;
    QLabel* m_summary = nullptr;
    int m_headerH = 0;
    // Personagem escolhido na aba Elenco: a aba Cenas acende só as dele.
    int m_focusCast = -1;
    int m_hover = -1;
    QList<QRect> m_rowRects;
};
