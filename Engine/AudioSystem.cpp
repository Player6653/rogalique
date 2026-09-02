#include "pch.h"
#include "AudioSystem.h"
#include "Log.h"
#include <algorithm>

namespace
{
    // С запасом на реалистичный пик одновременных эффектов (удары/попадания в бою) — см. комментарий у m_soundPool
    // в AudioSystem.h, почему reserve() важен именно здесь, а не просто "на всякий случай".
    constexpr std::size_t SOUND_POOL_RESERVED_CAPACITY = 32;
    // Длительность кроссфейда между треками (см. AudioSystem::playMusic/update) — достаточно, чтобы смена волны
    // арены не звучала обрывом, но не настолько долго, чтобы два трека заметно "плыли" друг поверх друга.
    const sf::Time MUSIC_CROSSFADE_DURATION = sf::seconds(1.2f);
} // namespace

AudioSystem::AudioSystem()
{
    m_soundPool.reserve(SOUND_POOL_RESERVED_CAPACITY);
}

AudioSystem& AudioSystem::instance()
{
    static AudioSystem instance;
    return instance;
}

bool AudioSystem::playMusic(const std::string& path, bool loop)
{
    MusicPlayer& current = m_musicPlayers[m_activeMusicIndex];
    bool hasCurrentTrack = current.active && current.music.getStatus() != sf::Music::Stopped;

    int nextIndex = 1 - m_activeMusicIndex;
    MusicPlayer& next = m_musicPlayers[nextIndex];
    if (!next.music.openFromFile(path)) {
        LOG_WARN("AudioSystem: не удалось загрузить музыку \"" + path + "\"");
        return false;
    }
    next.music.setLoop(loop);

    if (!hasCurrentTrack) {
        // Ничего не играет (первый трек за сцену, либо предыдущий уже сам доиграл до конца без loop) — включаем
        // сразу на полную громкость, кроссфейдить не с чем.
        next.music.setVolume(m_musicVolume * 100.f);
        next.music.play();
        next.active = true;
        current.active = false;
        current.music.stop();
        m_activeMusicIndex = nextIndex;
        m_crossfading = false;
        LOG_INFO("AudioSystem: фоновая музыка \"" + path
                 + "\" запущена без кроссфейда (loop=" + (loop ? std::string("true") : std::string("false")) + ")");
        return true;
    }

    // Кроссфейд: новый плеер стартует с нуля и нарастает, старый одновременно угасает — оба физически звучат
    // одновременно MUSIC_CROSSFADE_DURATION, update() ведёт оба таймера каждый кадр.
    next.music.setVolume(0.f);
    next.music.play();
    next.active = true;
    m_activeMusicIndex = nextIndex;
    m_crossfading = true;
    m_crossfadeElapsed = sf::Time::Zero;
    m_crossfadeDuration = MUSIC_CROSSFADE_DURATION;
    LOG_INFO("AudioSystem: кроссфейд на \"" + path + "\" (loop=" + (loop ? std::string("true") : std::string("false")) + ")");
    return true;
}

void AudioSystem::stopMusic()
{
    m_musicPlayers[0].music.stop();
    m_musicPlayers[1].music.stop();
    m_musicPlayers[0].active = false;
    m_musicPlayers[1].active = false;
    m_crossfading = false;
}

void AudioSystem::update(sf::Time dt)
{
    if (!m_crossfading) {
        return;
    }

    m_crossfadeElapsed += dt;
    float fraction = std::min(1.f, m_crossfadeElapsed.asSeconds() / m_crossfadeDuration.asSeconds());

    MusicPlayer& fadingIn = m_musicPlayers[m_activeMusicIndex];
    MusicPlayer& fadingOut = m_musicPlayers[1 - m_activeMusicIndex];
    fadingIn.music.setVolume(m_musicVolume * 100.f * fraction);
    fadingOut.music.setVolume(m_musicVolume * 100.f * (1.f - fraction));

    if (fraction >= 1.f) {
        fadingOut.music.stop();
        fadingOut.active = false;
        m_crossfading = false;
    }
}

bool AudioSystem::loadSound(const std::string& name, const std::string& path)
{
    sf::SoundBuffer buffer;
    if (!buffer.loadFromFile(path)) {
        LOG_WARN("AudioSystem: не удалось загрузить звук \"" + path + "\" (name=\"" + name + "\")");
        return false;
    }
    m_soundBuffers[name] = std::move(buffer);
    return true;
}

void AudioSystem::playSound(const std::string& name)
{
    auto it = m_soundBuffers.find(name);
    if (it == m_soundBuffers.end()) {
        return;
    }

    // Ищем в пуле проигрыватель, который сейчас ничего не играет, и переиспользуем его.
    for (auto& sound : m_soundPool) {
        if (sound.getStatus() != sf::Sound::Playing) {
            sound.setBuffer(it->second);
            sound.setVolume(m_effectsVolume * 100.f);
            sound.play();
            return;
        }
    }

    // Свободных не нашлось — все заняты одновременно играющими эффектами, добавляем новый. Пока их не больше
    // SOUND_POOL_RESERVED_CAPACITY (см. конструктор выше), push_back гарантированно не реаллоцирует вектор и не
    // задевает уже играющие sf::Sound; если резерва когда-нибудь не хватит — предупреждаем в лог, а не молчим.
    if (m_soundPool.size() >= m_soundPool.capacity()) {
        LOG_WARN("AudioSystem: пул эффектов превысил зарезервированную ёмкость (" + std::to_string(m_soundPool.capacity())
                 + ") — возможен слышимый обрыв уже играющих звуков при добавлении нового");
    }
    m_soundPool.emplace_back();
    m_soundPool.back().setBuffer(it->second);
    m_soundPool.back().setVolume(m_effectsVolume * 100.f);
    m_soundPool.back().play();
}

void AudioSystem::setMusicVolume(float volume01)
{
    m_musicVolume = volume01;
    // Во время кроссфейда громкость обоих плееров каждый кадр всё равно пересчитывает update() (доля от
    // m_musicVolume) — трогать её здесь смысла нет, только собьёт текущую долю затухания/нарастания.
    if (!m_crossfading) {
        m_musicPlayers[m_activeMusicIndex].music.setVolume(m_musicVolume * 100.f);
    }
}
