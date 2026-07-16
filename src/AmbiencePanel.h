#pragma once

#include <QFrame>
#include <QString>

class AmbienceManager;
class QListWidget;
class QListWidgetItem;
class QSlider;
class QToolButton;
class QLabel;

// Painel flutuante de Som Imersivo. Mostra a lista de tracks (lida pela
// AmbienceManager direto da pasta de áudio), permite play/pause e ajuste de
// volume. Auto-fecha ao clicar fora.
class AmbiencePanel : public QFrame
{
    Q_OBJECT
public:
    explicit AmbiencePanel(AmbienceManager* manager, QWidget* parent = nullptr);

    // Abre o painel próximo ao retângulo do botão (em coords globais).
    void showNear(const QRect& anchorGlobal);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onTracksChanged();
    void onActiveTrackChanged(const QString& id);
    void onPlaybackStateChanged(bool playing);
    void onVolumeChanged(int percent);
    void applyTheme();

private:
    void buildUi();
    void rebuildList();
    void selectIdInList(const QString& id);
    void refreshPlayIcon(bool playing);

    AmbienceManager* m_manager;
    QListWidget* m_list = nullptr;
    QToolButton* m_playBtn = nullptr;
    QSlider* m_volume = nullptr;
    QLabel* m_volumeLabel = nullptr;
    QLabel* m_pathLabel = nullptr;
    bool m_syncingList = false;
};
