#pragma once

#include <QObject>
#include <QString>
#include <QVector>

class QMediaPlayer;
class QAudioOutput;
class QTimer;
class QVariantAnimation;

// Tocador de som ambiente. Varre uma pasta de áudios (assets/ambience-sounds
// no dev, ao lado do exe em produção) e expõe a lista de tracks pra UI. Tudo
// dinâmico — se o usuário largar um .wav/.mp3 novo na pasta, refresh() detecta.
//
// Desde 14/11/2026 toca uma MISTURA: até 1 música + até 3 sons ao mesmo
// tempo, cada um com volume próprio, por baixo de um volume geral. Os sons de
// fábrica têm categoria fixa (tabela no .cpp); arquivo que o usuário largar
// na pasta cai em "Suas músicas" e conta como música. Persiste mistura,
// volumes e o volume geral em QSettings.
class AmbienceManager : public QObject {
    Q_OBJECT
public:
    enum Category { Nature, RainSea, Places, Calm, Adventure, Suspense, Mine };
    static constexpr int kCategoryCount = 7;
    static constexpr int kMaxSounds = 3;  // além da música

    struct Track {
        QString id;        // basename do arquivo (sem extensão)
        QString name;      // título limpo: "Into the Wild"
        QString author;    // "Ebunny", "LogicMoon", "P.H.L" ou vazio
        QString filePath;  // absoluto
        QString coverPath; // imagem com o mesmo nome ao lado do áudio, se houver
        Category category = Mine;
        bool music = true; // música troca a outra; som soma
    };

    struct MixItem {
        QString id;
        int volume = 50; // 0..100, relativo ao volume geral
    };

    enum class ToggleResult { Added, Removed, Replaced, Full };

    explicit AmbienceManager(QObject* parent = nullptr);
    ~AmbienceManager() override;

    void refresh();

    const QVector<Track>& tracks() const { return m_tracks; }
    const Track* track(const QString& id) const;
    QVector<const Track*> tracksIn(Category c) const;
    static QString categoryName(Category c);
    QString tracksDirectory() const { return m_dir; }
    bool tracksDirectoryExists() const;

    const QVector<MixItem>& mix() const { return m_mix; }
    bool inMix(const QString& id) const;
    int volumePercent() const { return m_volume; }     // volume geral 0..100
    bool isPlaying() const { return m_playing; }

    // Desligar sozinho: 0 = desligado; senão minutos (15/30/60).
    int sleepMinutes() const { return m_sleepMinutes; }
    void cycleSleepTimer();

    // Misturas da mesa: as 3 prontas (só as que têm todos os arquivos na
    // pasta) e, depois delas, as que o usuário salvou.
    struct Preset { QString name; QVector<MixItem> items; bool user = false; };
    QVector<Preset> presets() const;

    // Misturas do usuário (14/11/2026). Salvar com um nome que já existe
    // substitui aquela mistura — é assim que se atualiza uma.
    void saveUserPreset(const QString& name);
    // Devolve a posição de onde saiu (pra desfazer), ou -1.
    int removeUserPreset(const QString& name, Preset* removed = nullptr);
    void restoreUserPreset(const Preset& p, int index);
    bool renameUserPreset(const QString& from, const QString& to);
    bool hasUserPreset(const QString& name) const;
    QString suggestPresetName() const;

public slots:
    ToggleResult toggleInMix(const QString& id);
    void removeFromMix(const QString& id);
    void setMix(const QVector<MixItem>& items);
    void setLayerVolume(const QString& id, int percent);
    void setVolumePercent(int percent);
    void play();
    void pause();
    void togglePlay();

signals:
    void tracksChanged();
    void mixChanged();
    void layerVolumeChanged(const QString& id, int percent);
    void volumeChanged(int percent);
    void playbackStateChanged(bool playing);
    void sleepTimerChanged(int minutes);
    void presetsChanged();

private:
    struct Voice {
        QString id;
        QMediaPlayer* player = nullptr;
        QAudioOutput* audio = nullptr;
    };

    void resolveDirectory();
    void loadFromSettings();
    void saveToSettings() const;
    void saveUserPresets() const;
    int userPresetIndex(const QString& name) const;
    void pruneMissing();
    void syncVoices();
    void applyVolumes();
    void setPlaying(bool on);
    Voice* voiceFor(const QString& id);

    QString m_dir;
    QVector<Track> m_tracks;
    QVector<MixItem> m_mix;
    QVector<Voice> m_voices;
    QVector<Preset> m_userPresets;
    int m_volume = 45; // 0..100
    bool m_playing = false;

    int m_sleepMinutes = 0;
    QTimer* m_sleepTimer = nullptr;
    QVariantAnimation* m_fade = nullptr;
    double m_fadeFactor = 1.0;
};
