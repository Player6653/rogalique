#include "ArenaWaveComponent.h"
#include "AudioSystem.h"
#include "GameObject.h"
#include "HealthComponent.h"
#include "Log.h"

ArenaWaveComponent::ArenaWaveComponent(std::vector<std::vector<WaveEnemySpec>> waves, std::vector<std::string> musicPaths,
    std::vector<sf::Vector2f> spawnPoints, std::function<GameObject*(const std::string&, sf::Vector2f)> spawnEnemy,
    std::function<void()> onAllWavesCleared, sf::Time victoryDelay, sf::Time spawnStagger)
    : m_waves(std::move(waves)),
      m_musicPaths(std::move(musicPaths)),
      m_spawnPoints(std::move(spawnPoints)),
      m_spawnEnemy(std::move(spawnEnemy)),
      m_onAllWavesCleared(std::move(onAllWavesCleared)),
      m_victoryDelay(victoryDelay),
      m_spawnStagger(spawnStagger)
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
    if (!m_pendingSpawns.empty()) {
        return false;
    }
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
    m_pendingSpawns.clear();

    if (index < m_musicPaths.size() && !m_musicPaths[index].empty()) {
        AudioSystem::instance().playMusic(m_musicPaths[index]);
    }

    // Сам спавн — не сразу (см. класс-комментарий spawnStagger в .h), а откладывается в очередь m_pendingSpawns,
    // которую update() ниже разбирает по одному бойцу за spawnStagger — точки спавна вычисляются здесь и сейчас
    // (по модулю m_spawnPoints.size(), тот же порядок, что был бы при немедленном спавне), чтобы результат не
    // зависел от того, когда именно очередь будет разобрана.
    std::size_t spawnedSoFar = 0;
    for (const WaveEnemySpec& spec : m_waves[index]) {
        for (int i = 0; i < spec.count; ++i) {
            if (m_spawnPoints.empty()) {
                break;
            }
            sf::Vector2f position = m_spawnPoints[spawnedSoFar % m_spawnPoints.size()];
            ++spawnedSoFar;
            m_pendingSpawns.push_back({spec.kind, position});
        }
    }
    // Первый боец волны — сразу на этом же кадре (0 задержки), не спустя ещё один spawnStagger вникуда — иначе
    // между открытием волны (сменой музыки выше) и появлением хоть кого-то была бы заметная пустая пауза.
    m_spawnStaggerRemaining = sf::Time::Zero;

    LOG_INFO("ArenaWaveComponent: волна " + std::to_string(index + 1) + "/" + std::to_string(m_waves.size())
             + " началась, бойцов в очереди " + std::to_string(m_pendingSpawns.size()));
}

void ArenaWaveComponent::update(sf::Time dt)
{
    // Задержка перед onAllWavesCleared (см. класс-комментарий victoryDelay в .h) — тикает независимо от обычной
    // проверки волн ниже, m_currentWave к этому моменту уже выставлен за границу m_waves и дальше не используется.
    if (m_victoryPending) {
        m_victoryDelayRemaining -= dt;
        if (m_victoryDelayRemaining <= sf::Time::Zero) {
            m_victoryPending = false;
            if (m_onAllWavesCleared) {
                m_onAllWavesCleared();
            }
        }
        return;
    }

    if (m_currentWave < 0 || static_cast<std::size_t>(m_currentWave) >= m_waves.size()) {
        return;
    }

    // Разбираем очередь спавна текущей волны, по одному бойцу за spawnStagger (0 — все в один кадр, как раньше,
    // если конкретной волне/вызову staggering не задан). Пока очередь не опустеет, currentWaveCleared() ниже
    // всё равно вернёт false (см. её комментарий в .h), так что дальше по функции не пройдём.
    if (!m_pendingSpawns.empty()) {
        m_spawnStaggerRemaining -= dt;
        while (!m_pendingSpawns.empty() && m_spawnStaggerRemaining <= sf::Time::Zero) {
            PendingSpawn next = m_pendingSpawns.front();
            m_pendingSpawns.erase(m_pendingSpawns.begin());
            GameObject* enemy = m_spawnEnemy(next.kind, next.position);
            if (enemy) {
                m_currentWaveEnemies.push_back(enemy);
            }
            m_spawnStaggerRemaining += m_spawnStagger;
        }
        return;
    }

    if (!currentWaveCleared()) {
        return;
    }
    std::size_t nextWave = static_cast<std::size_t>(m_currentWave) + 1;
    if (nextWave < m_waves.size()) {
        spawnWave(nextWave);
    } else {
        m_currentWave = static_cast<int>(m_waves.size()); // За пределы — обычная ветка выше больше не сработает.
        if (m_victoryDelay > sf::Time::Zero) {
            LOG_INFO("ArenaWaveComponent: последняя волна выбита, ждём анимацию смерти перед экраном победы");
            m_victoryPending = true;
            m_victoryDelayRemaining = m_victoryDelay;
        } else {
            LOG_INFO("ArenaWaveComponent: последняя волна выбита");
            if (m_onAllWavesCleared) {
                m_onAllWavesCleared();
            }
        }
    }
}

void ArenaWaveComponent::reset()
{
    m_currentWave = -1;
    m_currentWaveEnemies.clear();
    m_victoryPending = false;
    m_victoryDelayRemaining = sf::Time::Zero;
    m_pendingSpawns.clear();
    m_spawnStaggerRemaining = sf::Time::Zero;
}
