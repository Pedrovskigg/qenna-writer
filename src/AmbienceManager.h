#pragma once

#include <QObject>
#include <QString>
#include <QVector>

class QMediaPlayer;
class QAudioOutput;

// Tocador de som ambiente. Varre uma pasta de áudios (assets/ambience-sounds
// no dev, ao lado do exe em produção) e expõe a lista de tracks pra UI. Tudo
// dinâmico — se o usuário largar um .wav/.mp3 novo na pasta, refresh() detecta.
// Persiste track ativa + volume + estado em QSettings.
class AmbienceManager : public QObject {
    Q_OBJECT
public:
    struct Track {
        QString id;       // basename do arquivo (sem extensão)
        QString name;     // display: "Birds In Forest"
        QString filePath; // absoluto
    };

    explicit AmbienceManager(QObject* parent = nullptr);
    ~AmbienceManager() override;

    // Varre a pasta de sons. Idempotente, pode ser chamado sempre que o painel
    // abrir pra detectar arquivos novos.
    void refresh();

    const QVector<Track>& tracks() const { return m_tracks; }
    QString tracksDirectory() const { return m_dir; }
    bool tracksDirectoryExists() const;

    QString activeTrackId() const { return m_activeId; }
    int volumePercent() const { return m_volume; }     // 0..100
    bool isPlaying() const;

public slots:
    void setActiveTrack(const QString& id);
    void setVolumePercent(int percent);
    void play();
    void pause();
    void stop();
    void togglePlay();

signals:
    void tracksChanged();
    void activeTrackChanged(QString id);
    void volumeChanged(int percent);
    void playbackStateChanged(bool playing);

private:
    void resolveDirectory();
    void loadFromSettings();
    void saveToSettings() const;
    void ensurePlayer();
    void loadSourceForActive();

    QString m_dir;
    QVector<Track> m_tracks;
    QString m_activeId;
    int m_volume = 45; // 0..100

    QMediaPlayer* m_player = nullptr;
    QAudioOutput* m_audio = nullptr;
};
