#pragma once

#include "AmbienceManager.h"

#include <QElapsedTimer>
#include <QFrame>
#include <QRect>
#include <QString>
#include <QVector>

#include <functional>

class CarouselViewport;
class QLabel;
class QLineEdit;
class QScrollArea;
class QSlider;
class QTimer;
class QVBoxLayout;
class QHBoxLayout;

namespace AmbienceUi {
class Btn;
class Tile;
class MineTile;
class Row;
class Stack;
class ElideLabel;
}

// Painel flutuante de Som Imersivo (rework de 14/11/2026, "Cartões +
// Camadas" do lab concepts/som-imersivo). Três telas no mesmo painel:
//  - Cartões: as 6 categorias ilustradas (+ "Suas músicas" se o usuário
//    largou arquivo próprio na pasta);
//  - Categoria: os sons de um cartão; música troca a que toca, som soma;
//  - Mesa: volume de cada camada, volume geral, misturas prontas e o
//    relógio de desligar sozinho.
// Embaixo, sempre, a barra da mistura (o que toca empilhado, play, Mesa).
// Auto-fecha ao clicar fora.
class AmbiencePanel : public QFrame
{
    Q_OBJECT
public:
    explicit AmbiencePanel(AmbienceManager* manager, QWidget* parent = nullptr);

    // Abre o painel próximo ao retângulo do botão (em coords globais).
    void showNear(const QRect& anchorGlobal, Qt::Edge barSide = Qt::TopEdge);

    // Relógio das animações das vinhetas e do equalizador (s). Só anda
    // enquanto toca, pra cena congelar no pause em vez de pular.
    double animTime() const { return m_t; }
    AmbienceManager* manager() const { return m_manager; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void hideEvent(QHideEvent* e) override;

private slots:
    void applyTheme();

private:
    enum class View { Grid, Category, Mesa };

    void buildUi();
    QWidget* buildGridPage();
    QWidget* buildCategoryPage();
    QWidget* buildMesaPage();
    QWidget* buildBar();

    void setView(View v, AmbienceManager::Category cat = AmbienceManager::Nature);
    void rebuildCategoryRows();
    void rebuildLayers();
    void rebuildPresets(const QString& focusName = QString());
    void refreshPresetState();
    void startNaming(const QString& renaming);
    void finishNaming(bool accept);
    void refreshBar();
    void refreshTiles();
    void refreshTimer();
    void refreshPlaying();
    void updateAnimTimer();
    void tick();
    void relayout();
    void showToast(const QString& text, std::function<void()> undo = {});

    AmbienceManager* m_manager;
    View m_view = View::Grid;
    AmbienceManager::Category m_cat = AmbienceManager::Nature;

    QVBoxLayout* m_root = nullptr;
    QWidget* m_gridPage = nullptr;
    QWidget* m_catPage = nullptr;
    QWidget* m_mesaPage = nullptr;

    // Cartões
    QVector<AmbienceUi::Tile*> m_tiles;
    AmbienceUi::MineTile* m_mineTile = nullptr;

    // Categoria
    AmbienceUi::Btn* m_catBack = nullptr;
    QLabel* m_catCount = nullptr;
    QLabel* m_catHint = nullptr;
    QScrollArea* m_catScroll = nullptr;
    QWidget* m_catList = nullptr;
    QVector<AmbienceUi::Row*> m_rows;

    // Mesa
    AmbienceUi::Btn* m_timerBtn = nullptr;
    QWidget* m_presetBox = nullptr;
    CarouselViewport* m_presetCarousel = nullptr;
    QVector<QPair<AmbienceUi::Btn*, QStringList>> m_presetPills; // pílula + ids (ordenados)
    AmbienceUi::Btn* m_saveBtn = nullptr;
    QWidget* m_nameRow = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QString m_renaming; // vazio = salvando uma nova
    QString m_lastSavedName;
    QWidget* m_layersBox = nullptr;
    QLabel* m_mesaEmpty = nullptr;
    QSlider* m_master = nullptr;
    QLabel* m_masterNum = nullptr;

    // Barra da mistura
    AmbienceUi::Stack* m_stack = nullptr;
    AmbienceUi::ElideLabel* m_barTitle = nullptr;
    QLabel* m_barSub = nullptr;
    AmbienceUi::Btn* m_barPlay = nullptr;
    AmbienceUi::Btn* m_barMesa = nullptr;

    QLabel* m_toast = nullptr;
    std::function<void()> m_undo;
    QTimer* m_toastTimer = nullptr;
    QTimer* m_anim = nullptr;
    QElapsedTimer m_clock;
    double m_t = 0;

    QRect m_anchor;
    Qt::Edge m_barSide = Qt::TopEdge;
};
