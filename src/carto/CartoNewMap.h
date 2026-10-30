#pragma once

// Janela "Novo mapa" (29/10/2026, desenho aprovado no artifact "Mesa do cartógrafo"):
// nome, tamanho, estilo e bioma dominante, com a prévia pintada pelo próprio gerador
// (a mesma semente dá o mesmo mapa). Abre no primeiro mapa do projeto, no "Novo mapa…"
// e no "Regenerar mapa". Mora em cima do painel, com o mapa escurecido atrás.

#include "CartoCore.h"

#include <QImage>
#include <QPointer>
#include <QWidget>

#include <atomic>
#include <functional>
#include <memory>

class QButtonGroup;
class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QSpacerItem;
class QSpinBox;
class QThreadPool;
class QTimer;
class QToolButton;

namespace carto {

class NewMapPreview;
class NewMapTile;

struct NewMapChoice {
    QString name;                   // vazio = o nome sugerido pela semente
    int seed = 0, size = 1, water = 60;
    QString type = QStringLiteral("continent"), climate = QStringLiteral("mixed");
    bool info = true;               // gerar territórios e locais
    std::shared_ptr<World> world;   // o mundo da prévia, quando já saiu com essas mesmas escolhas
};

class NewMap : public QWidget {
    Q_OBJECT
public:
    enum class Mode { First, New, Regen };
    explicit NewMap(QWidget* parent);
    ~NewMap() override;

    // current/currentName: o mapa aberto (o Regenerar parte dele); terrainStyle: "mira" ou "rich", a roupa do Terreno
    void open(Mode mode, const World* current, const QString& currentName, const QString& terrainStyle);
    void dismiss();
    Mode mode() const { return m_mode; }
    void applyTheme();

    std::function<void(const NewMapChoice&, Mode)> accepted;
    std::function<void()> dismissed;

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent*) override;   // segura o clique: o mapa atrás não recebe
    void wheelEvent(QWheelEvent*) override;
    void keyPressEvent(QKeyEvent*) override;

private:
    void build();
    void relayout();
    void sync();
    void changed(bool thumbs);
    void startPreview();
    void startThumbs();
    void accept();
    NewMapChoice choice() const;
    QString key() const;   // as escolhas que mudam o mundo, pra saber se a prévia ainda vale

    Mode m_mode = Mode::New;
    QString m_style = QStringLiteral("rich");
    QFrame* m_card = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_sub = nullptr;
    QToolButton* m_close = nullptr;
    NewMapPreview* m_preview = nullptr;
    QLabel* m_metaSize = nullptr;
    QLabel* m_metaSeed = nullptr;
    QLineEdit* m_name = nullptr;
    QButtonGroup* m_sizes = nullptr;
    QButtonGroup* m_types = nullptr;
    QButtonGroup* m_climates = nullptr;
    QList<NewMapTile*> m_typeTiles;
    QLabel* m_sizeHint = nullptr;
    QLabel* m_typeHint = nullptr;
    QLabel* m_climateHint = nullptr;
    QLabel* m_waterCap = nullptr;
    QLabel* m_waterValue = nullptr;
    QSlider* m_water = nullptr;
    QSpinBox* m_seed = nullptr;
    QToolButton* m_dice = nullptr;
    QToolButton* m_info = nullptr;
    QPushButton* m_go = nullptr;
    QWidget* m_left = nullptr;
    QWidget* m_right = nullptr;
    QLabel* m_nameHint = nullptr;
    QList<QSpacerItem*> m_gaps;   // respiros entre os campos (apertam em tela baixa)

    QTimer* m_debounce = nullptr;
    QThreadPool* m_pool = nullptr;
    std::shared_ptr<std::atomic<bool>> m_cancel, m_thumbCancel;
    int m_gen = 0, m_thumbGen = 0;
    bool m_thumbsDirty = true;
    bool m_loading = false;   // as escolhas vêm de fora (abrir): não dispara prévia a cada campo
    std::shared_ptr<World> m_ready;   // o mundo que a prévia mostra
    QString m_readyKey;
};

} // namespace carto
