#pragma once
#include "EngineExport.h"
#include <SFML/Audio.hpp>
#include <map>
#include <string>
#include <vector>

// Синглтон единая точка воспроизведения звука.
class ENGINE_API AudioSystem {
public:
    static AudioSystem& instance();

    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

    // Открывает поток и запускает фоновую музыку; loop управляет зацикливанием. Если что-то уже играет — не режет
    // резким обрывом, а кроссфейдит (см. update()): новый трек нарастает с нуля, пока старый одновременно угасает.
    bool playMusic(const std::string& path, bool loop = true);
    void stopMusic();

    // Продвигает кроссфейд между треками — звать раз в кадр (см. Engine::run()), независимо от паузы мира: музыка
    // и так уже играет вне зависимости от GameWorld::isPaused(), кроссфейду тоже незачем на ней замирать.
    void update(sf::Time dt);

    // Короткие звуковые эффекты грузятся в буфер целиком под именем name и проигрываются по нему.
    bool loadSound(const std::string& name, const std::string& path);
    void playSound(const std::string& name);

    // Громкость в долях [0..1].
    float getMusicVolume() const
    {
        return m_musicVolume;
    }
    void setMusicVolume(float volume01);
    float getEffectsVolume() const
    {
        return m_effectsVolume;
    }
    void setEffectsVolume(float volume01)
    {
        m_effectsVolume = volume01;
    }

private:
    AudioSystem();

    // Кроссфейд держит два независимых плеера вместо одного (см. playMusic/update в .cpp) — пока новый трек
    // нарастает, старый должен физически продолжать звучать (угасая), одним sf::Music это не сделать.
    struct MusicPlayer {
        sf::Music music;
        bool active = false;
    };
    MusicPlayer m_musicPlayers[2];
    int m_activeMusicIndex = 0;
    bool m_crossfading = false;
    sf::Time m_crossfadeElapsed;
    sf::Time m_crossfadeDuration;

    std::map<std::string, sf::SoundBuffer> m_soundBuffers;

    // Пул проигрывателей эффектов — ёмкость резервируется заранее в конструкторе (см. .cpp), чтобы playSound() не
    // могла спровоцировать reallocation std::vector<sf::Sound> прямо во время боя: sf::Sound не умеет двигаться
    // (только копироваться вместе с созданием нового OpenAL source), так что реаллокация при добавлении нового
    // элемента незаметно пересоздавала бы источники ВСЕХ уже играющих в этот момент эффектов — слышимый щелчок/
    // обрыв звука именно в моменты пиковой нагрузки.
    std::vector<sf::Sound> m_soundPool;

    float m_musicVolume = 1.f;
    float m_effectsVolume = 1.f;
};
