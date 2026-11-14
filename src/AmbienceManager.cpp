#include "AmbienceManager.h"
#include "AppPaths.h"

#include <QAudioOutput>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QMediaPlayer>
#include <QRegularExpression>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QVariantAnimation>

namespace {
const QStringList kAudioExtensions = {
    QStringLiteral("*.wav"),
    QStringLiteral("*.mp3"),
    QStringLiteral("*.ogg"),
    QStringLiteral("*.m4a"),
    QStringLiteral("*.flac"),
};

struct Known { AmbienceManager::Category cat; bool music; };

// Os sons que vêm com o app (basename do arquivo -> categoria). Arquivo fora
// da tabela é do usuário: "Suas músicas", tratado como música.
const QHash<QString, Known>& knownTracks()
{
    using C = AmbienceManager::Category;
    static const QHash<QString, Known> t = {
        // Natureza (sons)
        {QStringLiteral("Birds_In_Forest"), {C::Nature, false}},
        {QStringLiteral("Morning_Birds"), {C::Nature, false}},
        {QStringLiteral("Wind_Forest"), {C::Nature, false}},
        {QStringLiteral("In_The_Wild"), {C::Nature, false}},
        {QStringLiteral("Day_In_The_Park"), {C::Nature, false}},
        {QStringLiteral("Crickets_Night"), {C::Nature, false}},
        {QStringLiteral("Campfire"), {C::Nature, false}},
        {QStringLiteral("Sea_Birds"), {C::Nature, false}},
        // Chuva e mar (sons)
        {QStringLiteral("Light_Rain"), {C::RainSea, false}},
        {QStringLiteral("Heavy_Rain"), {C::RainSea, false}},
        {QStringLiteral("Thunder_Rain"), {C::RainSea, false}},
        {QStringLiteral("Thunder_Rain_2"), {C::RainSea, false}},
        {QStringLiteral("Sea_Waves"), {C::RainSea, false}},
        {QStringLiteral("Angry_Sea"), {C::RainSea, false}},
        // Lugares (três sons + três músicas)
        {QStringLiteral("City_Traffic"), {C::Places, false}},
        {QStringLiteral("Urban_City"), {C::Places, false}},
        {QStringLiteral("Spaceship_Engine"), {C::Places, false}},
        {QStringLiteral("Space_Bells"), {C::Places, true}},
        {QStringLiteral("Cinematic_Room"), {C::Places, true}},
        {QStringLiteral("Cyber_Techno"), {C::Places, true}},
        // Música calma
        {QStringLiteral("Inspiration_Piano"), {C::Calm, true}},
        {QStringLiteral("Minimal_Piano"), {C::Calm, true}},
        {QStringLiteral("Morning_Guitar"), {C::Calm, true}},
        {QStringLiteral("Wooden_Heart"), {C::Calm, true}},
        {QStringLiteral("Blues_From_Deep"), {C::Calm, true}},
        {QStringLiteral("Cello_Tears"), {C::Calm, true}},
        {QStringLiteral("LogicMoon_-_Coming_Home"), {C::Calm, true}},
        {QStringLiteral("LogicMoon_-_Guitambient"), {C::Calm, true}},
        {QStringLiteral("LogicMoon_-_Immersion"), {C::Calm, true}},
        {QStringLiteral("LogicMoon_-_Sleeping_Angel"), {C::Calm, true}},
        {QStringLiteral("LogicMoon_-_The_Lighthouse"), {C::Calm, true}},
        {QStringLiteral("Dunes_by_LogicMoon"), {C::Calm, true}},
        {QStringLiteral("Ebunny - Enchanted Guitar"), {C::Calm, true}},
        {QStringLiteral("P.H.L - The Spring Is Coming"), {C::Calm, true}},
        // Aventura
        {QStringLiteral("In_Action"), {C::Adventure, true}},
        {QStringLiteral("Hunters_by_LogicMoon"), {C::Adventure, true}},
        {QStringLiteral("Slipwork_by_LogicMoon"), {C::Adventure, true}},
        {QStringLiteral("Ebunny - Pirate Adventure"), {C::Adventure, true}},
        {QStringLiteral("Ebunny - Into the Wild"), {C::Adventure, true}},
        {QStringLiteral("Ebunny - Naughty Princess"), {C::Adventure, true}},
        {QStringLiteral("Cello_Angry"), {C::Adventure, true}},
        // Suspense
        {QStringLiteral("Beyond_The_Grave"), {C::Suspense, true}},
        {QStringLiteral("Creepy_Tomb"), {C::Suspense, true}},
        {QStringLiteral("Haunted"), {C::Suspense, true}},
        {QStringLiteral("Horror_Piano"), {C::Suspense, true}},
        {QStringLiteral("Maniac_Killer"), {C::Suspense, true}},
        {QStringLiteral("In_The_Crime"), {C::Suspense, true}},
        {QStringLiteral("Suspense_Drone"), {C::Suspense, true}},
        {QStringLiteral("Suspense_Terror"), {C::Suspense, true}},
        {QStringLiteral("Ebunny - Halloween Darkness"), {C::Suspense, true}},
    };
    return t;
}

QString cleanTitle(QString s)
{
    s.replace(QChar('-'), QChar(' '));
    return s.simplified();
}

// "Ebunny - Into the Wild" -> ("Into the Wild", "Ebunny");
// "Dunes_by_LogicMoon" -> ("Dunes", "LogicMoon"); "Light_Rain" -> ("Light Rain", "").
void splitName(const QString& baseName, QString* title, QString* author)
{
    QString s = baseName;
    s.replace(QChar('_'), QChar(' '));
    s = s.simplified();
    const int dash = s.indexOf(QStringLiteral(" - "));
    if (dash > 0) {
        *author = s.left(dash).trimmed();
        *title = cleanTitle(s.mid(dash + 3));
        return;
    }
    static const QRegularExpression by(QStringLiteral("^(.+) by (.+)$"),
                                       QRegularExpression::CaseInsensitiveOption);
    const auto m = by.match(s);
    if (m.hasMatch()) {
        *title = cleanTitle(m.captured(1));
        *author = m.captured(2).trimmed();
        return;
    }
    *title = cleanTitle(s);
    author->clear();
}

constexpr int kFadeMs = 4000;
}

AmbienceManager::AmbienceManager(QObject* parent)
    : QObject(parent)
{
    resolveDirectory();
    loadFromSettings();

    m_sleepTimer = new QTimer(this);
    m_sleepTimer->setSingleShot(true);
    connect(m_sleepTimer, &QTimer::timeout, this, [this]() {
        if (!m_playing) {
            m_sleepMinutes = 0;
            emit sleepTimerChanged(0);
            return;
        }
        m_fade->stop();
        m_fade->setStartValue(1.0);
        m_fade->setEndValue(0.0);
        m_fade->start();
    });
    m_fade = new QVariantAnimation(this);
    m_fade->setDuration(kFadeMs);
    connect(m_fade, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        m_fadeFactor = v.toDouble();
        applyVolumes();
    });
    connect(m_fade, &QVariantAnimation::finished, this, [this]() {
        pause();
        m_fadeFactor = 1.0;
        applyVolumes();
        m_sleepMinutes = 0;
        emit sleepTimerChanged(0);
    });

    refresh();
}

AmbienceManager::~AmbienceManager()
{
    for (Voice& v : m_voices)
        if (v.player) v.player->stop();
}

void AmbienceManager::resolveDirectory()
{
    // Instalador copia a pasta pra {app}\ambience-sounds\, sem prefixo
    // "assets" — mesmo mismatch que existia no bundledImagePath do
    // Theme.cpp (o caminho de prod tinha "assets/" a mais e nunca batia
    // com o que o instalador entrega de verdade pro usuário final).
    const QString prod = appAssetsDir()
        + QStringLiteral("/ambience-sounds");
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

QString AmbienceManager::categoryName(Category c)
{
    switch (c) {
    case Nature: return tr("Natureza");
    case RainSea: return tr("Chuva e mar");
    case Places: return tr("Lugares");
    case Calm: return tr("Música calma");
    case Adventure: return tr("Aventura");
    case Suspense: return tr("Suspense");
    case Mine: return tr("Suas músicas");
    }
    return QString();
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
            splitName(t.id, &t.name, &t.author);
            t.filePath = fi.absoluteFilePath();
            for (const char* ext : {".jpg", ".jpeg", ".png"}) {
                const QString img = d.absoluteFilePath(t.id + QLatin1String(ext));
                if (QFileInfo::exists(img)) { t.coverPath = img; break; }
            }
            const auto it = knownTracks().constFind(t.id);
            if (it != knownTracks().constEnd()) {
                t.category = it->cat;
                t.music = it->music;
            }
            m_tracks.append(t);
        }
    }
    emit tracksChanged();
    pruneMissing();
}

const AmbienceManager::Track* AmbienceManager::track(const QString& id) const
{
    for (const Track& t : m_tracks)
        if (t.id == id) return &t;
    return nullptr;
}

QVector<const AmbienceManager::Track*> AmbienceManager::tracksIn(Category c) const
{
    QVector<const Track*> out;
    for (const Track& t : m_tracks)
        if (t.category == c) out.append(&t);
    return out;
}

bool AmbienceManager::inMix(const QString& id) const
{
    for (const MixItem& m : m_mix)
        if (m.id == id) return true;
    return false;
}

void AmbienceManager::pruneMissing()
{
    QVector<MixItem> kept;
    for (const MixItem& m : m_mix)
        if (track(m.id)) kept.append(m);
    if (kept.size() != m_mix.size()) {
        m_mix = kept;
        if (!m_tracks.isEmpty()) saveToSettings(); // pasta sumida não apaga a mistura salva
        if (m_mix.isEmpty()) setPlaying(false);
    }
    syncVoices();
    emit mixChanged();
}

void AmbienceManager::loadFromSettings()
{
    QSettings s;
    m_volume = qBound(0, s.value(QStringLiteral("ambience/volume"), 45).toInt(), 100);
    m_mix.clear();
    if (s.contains(QStringLiteral("ambience/mix"))) {
        const QStringList items = s.value(QStringLiteral("ambience/mix")).toStringList();
        for (const QString& it : items) {
            const int sep = it.lastIndexOf(QChar('|'));
            if (sep <= 0) continue;
            MixItem m;
            m.id = it.left(sep);
            m.volume = qBound(0, it.mid(sep + 1).toInt(), 100);
            m_mix.append(m);
        }
    } else {
        // Antes da mistura (até a 2.0) era uma faixa só, no volume geral.
        const QString old = s.value(QStringLiteral("ambience/activeTrack")).toString();
        if (!old.isEmpty()) m_mix.append({old, 100});
    }

    m_userPresets.clear();
    const QVariantList presets = s.value(QStringLiteral("ambience/userPresets")).toList();
    for (const QVariant& v : presets) {
        const QVariantMap m = v.toMap();
        Preset p;
        p.user = true;
        p.name = m.value(QStringLiteral("name")).toString().trimmed();
        for (const QString& it : m.value(QStringLiteral("items")).toStringList()) {
            const int sep = it.lastIndexOf(QChar('|'));
            if (sep <= 0) continue;
            p.items.append({it.left(sep), qBound(0, it.mid(sep + 1).toInt(), 100)});
        }
        if (!p.name.isEmpty() && !p.items.isEmpty()) m_userPresets.append(p);
    }
}

void AmbienceManager::saveUserPresets() const
{
    QVariantList list;
    for (const Preset& p : m_userPresets) {
        QStringList items;
        for (const MixItem& m : p.items)
            items << m.id + QChar('|') + QString::number(m.volume);
        QVariantMap m;
        m.insert(QStringLiteral("name"), p.name);
        m.insert(QStringLiteral("items"), items);
        list.append(m);
    }
    QSettings s;
    s.setValue(QStringLiteral("ambience/userPresets"), list);
}

int AmbienceManager::userPresetIndex(const QString& name) const
{
    const QString n = name.trimmed();
    for (int i = 0; i < m_userPresets.size(); ++i)
        if (m_userPresets[i].name.compare(n, Qt::CaseInsensitive) == 0) return i;
    return -1;
}

bool AmbienceManager::hasUserPreset(const QString& name) const
{
    return userPresetIndex(name) >= 0;
}

QString AmbienceManager::suggestPresetName() const
{
    for (int n = m_userPresets.size() + 1;; ++n) {
        const QString name = tr("Minha mistura %1").arg(n);
        if (!hasUserPreset(name)) return name;
    }
}

void AmbienceManager::saveUserPreset(const QString& name)
{
    const QString n = name.trimmed();
    if (n.isEmpty() || m_mix.isEmpty()) return;
    Preset p;
    p.user = true;
    p.name = n;
    p.items = m_mix;
    const int i = userPresetIndex(n);
    if (i >= 0) m_userPresets[i] = p;
    else m_userPresets.append(p);
    saveUserPresets();
    emit presetsChanged();
}

int AmbienceManager::removeUserPreset(const QString& name, Preset* removed)
{
    const int i = userPresetIndex(name);
    if (i < 0) return -1;
    if (removed) *removed = m_userPresets[i];
    m_userPresets.removeAt(i);
    saveUserPresets();
    emit presetsChanged();
    return i;
}

void AmbienceManager::restoreUserPreset(const Preset& p, int index)
{
    if (p.name.trimmed().isEmpty() || hasUserPreset(p.name)) return;
    Preset copy = p;
    copy.user = true;
    m_userPresets.insert(qBound(0, index, int(m_userPresets.size())), copy);
    saveUserPresets();
    emit presetsChanged();
}

bool AmbienceManager::renameUserPreset(const QString& from, const QString& to)
{
    const int i = userPresetIndex(from);
    const QString n = to.trimmed();
    if (i < 0 || n.isEmpty()) return false;
    const int clash = userPresetIndex(n);
    if (clash >= 0 && clash != i) return false;
    m_userPresets[i].name = n;
    saveUserPresets();
    emit presetsChanged();
    return true;
}

void AmbienceManager::saveToSettings() const
{
    QSettings s;
    QStringList items;
    for (const MixItem& m : m_mix)
        items << m.id + QChar('|') + QString::number(m.volume);
    s.setValue(QStringLiteral("ambience/mix"), items);
    s.setValue(QStringLiteral("ambience/volume"), m_volume);
}

AmbienceManager::Voice* AmbienceManager::voiceFor(const QString& id)
{
    for (Voice& v : m_voices)
        if (v.id == id) return &v;
    return nullptr;
}

void AmbienceManager::syncVoices()
{
    // Tira as vozes que saíram da mistura.
    for (int i = m_voices.size() - 1; i >= 0; --i) {
        if (inMix(m_voices[i].id)) continue;
        m_voices[i].player->stop();
        m_voices[i].player->deleteLater();
        m_voices[i].audio->deleteLater();
        m_voices.removeAt(i);
    }
    // Cria as que entraram. Só quando está tocando: abrir o app com uma
    // mistura salva não pode custar 4 decodificadores parados.
    for (const MixItem& m : m_mix) {
        if (!m_playing) break;
        if (voiceFor(m.id)) continue;
        const Track* t = track(m.id);
        if (!t) continue;
        Voice v;
        v.id = m.id;
        v.player = new QMediaPlayer(this);
        v.audio = new QAudioOutput(this);
        v.player->setAudioOutput(v.audio);
        v.player->setLoops(QMediaPlayer::Infinite);
        v.player->setSource(QUrl::fromLocalFile(t->filePath));
        m_voices.append(v);
        if (m_playing) v.player->play();
    }
    applyVolumes();
}

void AmbienceManager::applyVolumes()
{
    for (Voice& v : m_voices) {
        int layer = 50;
        for (const MixItem& m : m_mix)
            if (m.id == v.id) { layer = m.volume; break; }
        v.audio->setVolume(qBound(0.0, layer / 100.0 * m_volume / 100.0 * m_fadeFactor, 1.0));
    }
}

void AmbienceManager::setPlaying(bool on)
{
    if (on && m_mix.isEmpty()) on = false;
    if (on && m_fade && m_fade->state() == QAbstractAnimation::Running) {
        m_fade->stop();
        m_fadeFactor = 1.0;
        applyVolumes();
    }
    const bool changed = (on != m_playing);
    m_playing = on;
    if (on) syncVoices();
    for (Voice& v : m_voices) {
        if (on) v.player->play();
        else v.player->pause();
    }
    if (changed) emit playbackStateChanged(m_playing);
}

AmbienceManager::ToggleResult AmbienceManager::toggleInMix(const QString& id)
{
    const Track* t = track(id);
    if (!t) return ToggleResult::Removed;
    for (int i = 0; i < m_mix.size(); ++i) {
        if (m_mix[i].id != id) continue;
        m_mix.removeAt(i);
        syncVoices();
        if (m_mix.isEmpty()) setPlaying(false);
        saveToSettings();
        emit mixChanged();
        return ToggleResult::Removed;
    }

    ToggleResult result = ToggleResult::Added;
    if (t->music) {
        int vol = 60;
        for (int i = m_mix.size() - 1; i >= 0; --i) {
            const Track* o = track(m_mix[i].id);
            if (o && o->music) {
                vol = m_mix[i].volume; // a música nova herda o volume da que sai
                m_mix.removeAt(i);
                result = ToggleResult::Replaced;
            }
        }
        m_mix.prepend({id, vol});
    } else {
        int sounds = 0;
        for (const MixItem& m : m_mix) {
            const Track* o = track(m.id);
            if (o && !o->music) ++sounds;
        }
        if (sounds >= kMaxSounds) return ToggleResult::Full;
        m_mix.append({id, 50});
    }
    syncVoices();
    setPlaying(true);
    saveToSettings();
    emit mixChanged();
    return result;
}

void AmbienceManager::removeFromMix(const QString& id)
{
    if (inMix(id)) toggleInMix(id);
}

void AmbienceManager::setMix(const QVector<MixItem>& items)
{
    m_mix.clear();
    for (const MixItem& m : items)
        if (track(m.id) && !inMix(m.id)) m_mix.append(m);
    syncVoices();
    setPlaying(!m_mix.isEmpty());
    saveToSettings();
    emit mixChanged();
}

void AmbienceManager::setLayerVolume(const QString& id, int percent)
{
    const int v = qBound(0, percent, 100);
    for (MixItem& m : m_mix) {
        if (m.id != id) continue;
        if (m.volume == v) return;
        m.volume = v;
        applyVolumes();
        saveToSettings();
        emit layerVolumeChanged(id, v);
        return;
    }
}

void AmbienceManager::setVolumePercent(int percent)
{
    const int v = qBound(0, percent, 100);
    if (v == m_volume) return;
    m_volume = v;
    applyVolumes();
    saveToSettings();
    emit volumeChanged(m_volume);
}

void AmbienceManager::play() { setPlaying(true); }
void AmbienceManager::pause() { setPlaying(false); }
void AmbienceManager::togglePlay() { setPlaying(!m_playing); }

void AmbienceManager::cycleSleepTimer()
{
    switch (m_sleepMinutes) {
    case 0: m_sleepMinutes = 15; break;
    case 15: m_sleepMinutes = 30; break;
    case 30: m_sleepMinutes = 60; break;
    default: m_sleepMinutes = 0; break;
    }
    m_fade->stop();
    if (m_fadeFactor != 1.0) { m_fadeFactor = 1.0; applyVolumes(); }
    if (m_sleepMinutes > 0) m_sleepTimer->start(m_sleepMinutes * 60 * 1000);
    else m_sleepTimer->stop();
    emit sleepTimerChanged(m_sleepMinutes);
}

QVector<AmbienceManager::Preset> AmbienceManager::presets() const
{
    const QVector<Preset> all = {
        {tr("Café chuvoso"), {{QStringLiteral("Minimal_Piano"), 45},
                              {QStringLiteral("Light_Rain"), 60},
                              {QStringLiteral("Urban_City"), 35}}},
        {tr("Acampamento"), {{QStringLiteral("Campfire"), 55},
                             {QStringLiteral("Crickets_Night"), 45},
                             {QStringLiteral("Wind_Forest"), 35}}},
        {tr("Tempestade"), {{QStringLiteral("Suspense_Drone"), 40},
                            {QStringLiteral("Thunder_Rain"), 60},
                            {QStringLiteral("Angry_Sea"), 40}}},
    };
    QVector<Preset> out;
    for (const Preset& p : all) {
        bool ok = true;
        for (const MixItem& m : p.items)
            if (!track(m.id)) { ok = false; break; }
        if (ok) out.append(p);
    }
    // As do usuário entram mesmo se um arquivo sumiu: o setMix pula o que
    // não existe, e a mistura volta inteira se o arquivo voltar.
    out += m_userPresets;
    return out;
}
