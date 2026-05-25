#include "AmbienceManager.h"

#include <QAudioOutput>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMediaPlayer>
#include <QSettings>
#include <QUrl>

namespace {
const QStringList kAudioExtensions = {
    QStringLiteral("*.wav"),
    QStringLiteral("*.mp3"),
    QStringLiteral("*.ogg"),
    QStringLiteral("*.m4a"),
    QStringLiteral("*.flac"),
};

QString prettyName(const QString& baseName)
{
    QString s = baseName;
    s.replace(QChar('_'), QChar(' '));
    s.replace(QChar('-'), QChar(' '));
    s = s.simplified();
    return s;
}
}

AmbienceManager::AmbienceManager(QObject* parent)
    : QObject(parent)
{
    resolveDirectory();
    loadFromSettings();
    refresh();
}

AmbienceManager::~AmbienceManager()
{
    if (m_player) m_player->stop();
}

void AmbienceManager::resolveDirectory()
{
    const QString prod = QCoreApplication::applicationDirPath()
        + QStringLiteral("/assets/ambience-sounds");
    if (QDir(prod).exists()) {
        m_dir = prod;
        return;
    }
#ifdef DEV_ASSETS_DIR
    const QString dev = QString::fromUtf8(DEV_ASSETS_DIR)
        + QStringLiteral("/ambience-sounds");
    if (QDir(dev).exists()) {
        m_dir = dev;
        return;
    }
#endif
    m_dir = prod; // fallback — exibido como path mesmo se vazio
}

bool AmbienceManager::tracksDirectoryExists() const
{
    return !m_dir.isEmpty() && QDir(m_dir).exists();
}

void AmbienceManager::refresh()
{
    m_tracks.clear();
    if (tracksDirectoryExists()) {
        QDir d(m_dir);
        const QFileInfoList files = d.entryInfoList(kAudioExtensions, QDir::Files, QDir::Name);
        for (const QFileInfo& fi : files) {
            Track t;
            t.id = fi.completeBaseName();
            t.name = prettyName(t.id);
            t.filePath = fi.absoluteFilePath();
            m_tracks.append(t);
        }
    }
    emit tracksChanged();

    // Se a track ativa salva sumiu, escolhe a primeira.
    if (!m_activeId.isEmpty()) {
        bool stillExists = false;
        for (const Track& t : m_tracks) {
            if (t.id == m_activeId) { stillExists = true; break; }
        }
        if (!stillExists) {
            m_activeId.clear();
            if (m_player) m_player->setSource(QUrl());
        }
    }
    if (m_activeId.isEmpty() && !m_tracks.isEmpty()) {
        setActiveTrack(m_tracks.first().id);
    }
}

void AmbienceManager::loadFromSettings()
{
    QSettings s;
    m_activeId = s.value(QStringLiteral("ambience/activeTrack")).toString();
    m_volume = s.value(QStringLiteral("ambience/volume"), 45).toInt();
    if (m_volume < 0) m_volume = 0;
    if (m_volume > 100) m_volume = 100;
}

void AmbienceManager::saveToSettings() const
{
    QSettings s;
    s.setValue(QStringLiteral("ambience/activeTrack"), m_activeId);
    s.setValue(QStringLiteral("ambience/volume"), m_volume);
}

void AmbienceManager::ensurePlayer()
{
    if (m_player) return;
    m_player = new QMediaPlayer(this);
    m_audio = new QAudioOutput(this);
    m_player->setAudioOutput(m_audio);
    m_audio->setVolume(qBound(0.0, m_volume / 100.0, 1.0));
    m_player->setLoops(QMediaPlayer::Infinite);

    connect(m_player, &QMediaPlayer::playbackStateChanged,
            this, [this](QMediaPlayer::PlaybackState st) {
        emit playbackStateChanged(st == QMediaPlayer::PlayingState);
    });
}

void AmbienceManager::loadSourceForActive()
{
    if (!m_player) ensurePlayer();
    QString path;
    for (const Track& t : m_tracks) {
        if (t.id == m_activeId) { path = t.filePath; break; }
    }
    const QUrl url = path.isEmpty() ? QUrl() : QUrl::fromLocalFile(path);
    if (m_player->source() != url) {
        m_player->setSource(url);
    }
}

void AmbienceManager::setActiveTrack(const QString& id)
{
    if (id == m_activeId) return;
    m_activeId = id;
    ensurePlayer();
    const bool wasPlaying = isPlaying();
    loadSourceForActive();
    if (wasPlaying) m_player->play();
    saveToSettings();
    emit activeTrackChanged(m_activeId);
}

void AmbienceManager::setVolumePercent(int percent)
{
    const int v = qBound(0, percent, 100);
    if (v == m_volume) return;
    m_volume = v;
    if (m_audio) m_audio->setVolume(m_volume / 100.0);
    saveToSettings();
    emit volumeChanged(m_volume);
}

bool AmbienceManager::isPlaying() const
{
    return m_player && m_player->playbackState() == QMediaPlayer::PlayingState;
}

void AmbienceManager::play()
{
    if (m_activeId.isEmpty()) return;
    ensurePlayer();
    if (m_player->source().isEmpty()) loadSourceForActive();
    m_player->play();
}

void AmbienceManager::pause()
{
    if (m_player) m_player->pause();
}

void AmbienceManager::stop()
{
    if (m_player) m_player->stop();
}

void AmbienceManager::togglePlay()
{
    if (isPlaying()) pause();
    else play();
}
