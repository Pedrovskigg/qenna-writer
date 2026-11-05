#pragma once

// A janela "Nova galáxia": prévia ao lado, galáxia pronta, nome, estilo, braços, tamanho, sistemas, vida e semente.
// Mora em cima do painel do Mapa, como a janela Novo mapa, e é a outra aba dela.

#include "GalaxyDoc.h"

#include <QImage>
#include <QTimer>
#include <QWidget>

#include <array>
#include <functional>

class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QSpinBox;
class QToolButton;

namespace galaxy {

class NewGalaxy : public QWidget {
    Q_OBJECT
public:
    explicit NewGalaxy(QWidget* parent);
    enum class Mode { First, New, Regen };
    // regen: parte das escolhas da galáxia aberta
    void open(Mode mode, const GalOpts* current, int currentCount);
    void dismiss();
    void applyTheme();

    std::function<void(const GalOpts&, const QString& name, Mode)> accepted;
    std::function<void()> dismissed;
    std::function<void()> wantEarth;    // a pessoa escolheu Terra na chave de cima

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void keyPressEvent(QKeyEvent*) override;

private:
    void build();
    void sync();
    void change();
    void leavePreset();
    void preview();
    void previewMix();
    void setHand(bool on);
    void syncMix();
    void setMixShare(int n, double value);
    void mixMoved();
    QWidget* m_handBox = nullptr;
    class MixBar* m_mixBar = nullptr;
    QLabel* m_mixText = nullptr;
    class HabBar* m_natBar = nullptr;
    QToolButton* m_handCheck = nullptr;
    QToolButton* m_organicCheck = nullptr;
    QLineEdit* m_mixIn[7] {};
    QLabel* m_mixCount[7] {};
    QTimer m_mixTimer;
    int m_mixTok = 0, m_worlds = 0;
    std::array<int, 7> m_natural {};
    bool m_hasNatural = false, m_hand = false;
    std::vector<int> m_touched;
    QFrame* m_card = nullptr;
    QLabel* m_title = nullptr;
    QWidget* m_previewBox = nullptr;
    QLabel* m_info = nullptr;
    QToolButton *m_tabEarth = nullptr, *m_tabGal = nullptr;
    QComboBox *m_preset = nullptr, *m_style = nullptr;
    QLabel* m_presetNote = nullptr;
    QLineEdit* m_name = nullptr;
    QWidget* m_armsRow = nullptr;
    QLabel* m_armsV = nullptr;
    QSlider* m_arms = nullptr;
    QHash<int, QToolButton*> m_sizes;
    QSpinBox* m_count = nullptr;
    QLabel* m_lifeV = nullptr;
    QSlider* m_life = nullptr;
    QLineEdit* m_seed = nullptr;
    QLabel* m_note = nullptr;
    QPushButton* m_go = nullptr;
    QToolButton* m_close = nullptr;
    QTimer m_timer;
    QImage m_img;
    Mode m_mode = Mode::New;
    GalOpts m_d;
    QString m_nameText;
    int m_tok = 0;
    bool m_loading = false;
    friend class NewGalaxyPreview;
    friend class MixBar;
};

} // namespace galaxy
