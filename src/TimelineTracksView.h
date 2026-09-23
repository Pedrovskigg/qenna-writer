#pragma once
// Modo Trilhos da Timeline nova. Estrutura em colunas (uma por capítulo/cena,
// em ordem de leitura) com calha fixa à esquerda — os nomes das linhas e do
// elenco NUNCA rolam, porque vivem fora da área de conteúdo. Uma única barra
// horizontal é a fonte de verdade da rolagem: régua, linhas e elenco leem o
// valor dela na hora de pintar; nada é sincronizado entre si.
//
// Zoom = densidade semântica (Pontos / Títulos / Resumos): os capítulos se
// afastam ou se aproximam; texto, bolinhas e linhas mantêm o tamanho.

#include "TimelineTracksTypes.h"

#include <QPointer>
#include <QWidget>

class QScrollArea;
class QScrollBar;
class QSplitter;

namespace TracksDetail { class Ruler; class BandCanvas; class LaneCanvas; class CastCanvas; class Ghost; }

class TimelineTracksView : public QWidget {
    Q_OBJECT
public:
    enum Density { Dots = 0, Titles = 1, Summaries = 2 };

    explicit TimelineTracksView(QWidget* parent = nullptr);

    void setData(const Tracks::Data& data);
    const Tracks::Data& data() const { return m_data; }

    void setFilter(const Tracks::Filter& f);
    const Tracks::Filter& filter() const { return m_filter; }

    void setSelected(const QString& id);
    QString selected() const { return m_sel; }

    void setDensity(Density d);
    Density density() const { return m_density; }

    // Tamanho da faixa das linhas (divisória). -1 = automático.
    int  lanesPaneHeight() const;
    void setLanesPaneHeight(int h);

    // Rola até a coluna (usado pra abrir no capítulo do editor).
    void scrollToColumn(int col);

signals:
    void eventClicked(const QString& id);
    void backgroundClicked();
    void laneClicked(const QString& laneId);       // foca/desfoca a linha
    void characterClicked(const QString& charId);  // filtra/desfiltra
    void densityChangeRequested(int density);      // Ctrl + roda
    // Soltou um evento arrastado. laneId vazio = Soltos; col = coluna de
    // destino (-1 quando não se aplica).
    void eventDropped(const QString& id, const QString& laneId, int col);
    void createAtRequested(const QString& laneId, int col); // duplo clique no vazio
    void eventContextMenu(const QString& id, const QPoint& globalPos);
    void laneColorRequested(const QString& laneId, const QPoint& globalPos); // clique no traço colorido
    void laneContextMenu(const QString& laneId, const QPoint& globalPos);
    void layoutChanged();                          // divisória / densidade mudaram

protected:
    void resizeEvent(QResizeEvent* e) override;

private:
    friend class TracksDetail::Ruler;
    friend class TracksDetail::BandCanvas;
    friend class TracksDetail::LaneCanvas;
    friend class TracksDetail::CastCanvas;

    // ── geometria (coordenadas de conteúdo, antes da rolagem) ─────────────────
    static constexpr int kGutter = 180;
    static constexpr int kPadL   = 18;
    static constexpr int kRulerH = 34;
    qreal colW() const;
    qreal laneH() const;
    qreal lineOff() const;
    qreal xCol(int col) const { return kPadL + col * colW() + colW() / 2.0; }
    qreal yLane(int i) const { return 12.0 + i * laneH() + lineOff(); }
    bool  hasLoose() const;
    qreal looseTop() const { return 12.0 + m_data.lanes.size() * laneH() + 4.0; }
    qreal looseH() const { return hasLoose() ? 60.0 : 0.0; }
    qreal lanesContentH() const;
    qreal castContentH() const;
    qreal contentW() const { return kPadL * 2 + m_data.cols.size() * colW(); }
    int   sx() const;

    void refreshScroll();
    void repaintAll();
    void setHoverCol(int col);

    Tracks::Data   m_data;
    Tracks::Filter m_filter;
    QString        m_sel;
    Density        m_density = Titles;
    int            m_hoverCol = -1;
    QString        m_hoverCastChar;
    int            m_scrolledForDensity = -1;
    int            m_scrolledForEditorCol = -2;

    TracksDetail::Ruler*      m_ruler = nullptr;
    TracksDetail::LaneCanvas* m_lanes = nullptr;
    TracksDetail::CastCanvas* m_cast  = nullptr;
    QScrollArea* m_laneArea = nullptr;
    QScrollArea* m_castArea = nullptr;
    QSplitter*   m_split    = nullptr;
    QScrollBar*  m_hbar     = nullptr;
    QWidget*     m_hbarRow  = nullptr;
    int          m_wantLanesH = -1;
};
