#include "ArenaWaveComponent.h"
#include "AudioSystem.h"
#include "GameObject.h"
#include "HealthComponent.h"
#include "Log.h"

ArenaWaveComponent::ArenaWaveComponent(std::vector<std::vector<WaveEnemySpec>> waves, std::vector<std::string> musicPaths,
    std::vector<sf::Vector2f> spawnPoints, std::function<GameObject*(const std::string&, sf::Vector2f)> spawnEnemy,
    std::function<void()> onAllWavesCleared)
    : m_waves(std::move(waves)),
      m_musicPaths(std::move(musicPaths)),
      m_spawnPoints(std::move(spawnPoints)),
      m_spawnEnemy(std::move(spawnEnemy)),
      m_onAllWavesCleared(std::move(onAllWavesCleared))
{
}

void ArenaWaveComponent::start()
{
    if (m_currentWave >= 0) {
        return; // Уже идёт (или отыграно) — см. класс-комментарий.
    }
    if (m_waves.empty()) {
        return; // Та же защита, что у startAtWave() — иначе spawnWave(0) читал бы m_waves[0] за границей.
    }
    spawnWave(0);
}

bool ArenaWaveComponent::currentWaveCleared() const
{
    for (GameObject* enemy : m_currentWaveEnemies) {
        auto* health = enemy->getComponent<HealthComponent>();
        if (health && !health->isDead()) {
            return false;
        }
    }
    return true;
}

void ArenaWaveComponent::spawnWave(std::size_t index)
{
    m_currentWave = static_cast<int>(index);
    m_currentWaveEnemies.clear();

    if (index < m_musicPaths.size() && !m_musicPaths[index].empty()) {
        AudioSystem::instance().playMusic(m_musicPaths[index]);
    }

    std::size_t spawnedSoFar = 0;
    for (const WaveEnemySpec& spec : m_waves[index]) {
        for (int i = 0; i < spec.count; ++i) {
            if (m_spawnPoints.empty()) {
                break;
            }
            sf::Vector2f position = m_spawnPoints[spawnedSoFar % m_spawnPoints.size()];
            ++spawnedSoFar;
            GameObject* enemy = m_spawnEnemy(spec.kind, position);
            if (enemy) {
                m_currentWaveEnemies.push_back(enemy);
            }
        }
    }
    LOG_INFO("ArenaWaveComponent: волна " + std::to_string(index + 1) + "/" + std::to_string(m_waves.size())
             + " началась, бойцов " + std::to_string(m_currentWaveEnemies.size()));
}

void ArenaWaveComponent::update(sf::Time)
{
    if (m_currentWave < 0 || static_cast<std::size_t>(m_currentWave) >= m_waves.size()) {
        return;
    }
    if (!currentWaveCleared()) {
        return;
    }
    std::size_t nextWave = static_cast<std::size_t>(m_currentWave) + 1;
    if (nextWave < m_waves.size()) {
        spawnWave(nextWave);
    } else {
        LOG_INFO("ArenaWaveComponent: последняя волна выбита");
        m_currentWave = static_cast<int>(m_waves.size()); // За пределы — update() выше больше не сработает.
        if (m_onAllWavesCleared) {
            m_onAllWavesCleared();
        }
    }
}

void ArenaWaveComponent::reset()
{
    m_currentWave = -1;
    m_currentWaveEnemies.clear();
}
