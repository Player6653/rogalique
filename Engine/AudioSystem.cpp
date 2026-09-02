#include "pch.h"
#include "AudioSystem.h"
#include "Log.h"

namespace
{
    // С запасом на реалистичный пик одновременных эффектов (удары/попадания в бою) — см. комментарий у m_soundPool
    // в AudioSystem.h, почему reserve() важен именно здесь, а не просто "на всякий случай".
    constexpr std::size_t SOUND_POOL_RESERVED_CAPACITY = 32;
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
    if (!m_music.openFromFile(path)) {
        LOG_WARN("AudioSystem: не удалось загрузить музыку \"" + path + "\"");
        return false;
    }
    m_music.setLoop(loop);
    m_music.setVolume(m_musicVolume * 100.f);
    m_music.play();
    LOG_INFO("AudioSystem: фоновая музыка \"" + path + "\" запущена (loop=" + (loop ? std::string("true") : std::string("false"))
             + ")");
    return true;
}

void AudioSystem::stopMusic()
{
    m_music.stop();
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
    m_music.setVolume(m_musicVolume * 100.f);
}
