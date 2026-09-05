#pragma once
#include "IComponent.h"
#include <SFML/System/Vector2.hpp>
#include <functional>
#include <string>
#include <vector>

class GameObject;

// Волны монстров на арене выживания (см. SceneFacade::run() — арена грузится из Resources/Level/Arena.tmj тем же
// путём, что и комнаты подземелья, попадаем сюда через открытую главную дверь). Живёт на отдельном служебном
// GameObject, не на самой арене/двери — start()
// зовётся из DoorComponent::setOnOpened(), после этого update() сам следит, когда очередная волна выбита
// целиком (см. m_currentWaveEnemies — те же указатели, что вернул spawnEnemy, не уничтожаются при смерти, только
// помечаются HealthComponent::isDead(), так что хранить и проверять их напрямую безопасно), и запускает
// следующую (со сменой музыки) или, после последней, зовёт onAllWavesCleared (см. SceneFacade — экран победы).
class ArenaWaveComponent : public IComponent {
public:
    struct WaveEnemySpec {
        std::string kind; // "orc"/"soldier"/"slime1"/"slime2"/"slime3" — см. makeEnemyInstance в SceneFacade.cpp.
        int count = 0;
    };

    // spawnEnemy — фактическое создание+добавление в сцену (SceneFacade знает, как: makeEnemyInstance() +
    // GameWorld::spawnIn(), см. run()) — компонент сам ничего не создаёт, просто дирижирует. musicPaths — по
    // записи на волну, играется при её старте (пустая строка — не менять текущую музыку). spawnPoints — где
    // появляются бойцы волны, зациклено по модулю, если бойцов больше точек. victoryDelay — необязательная пауза
    // (0 по умолчанию, как раньше) между тем, как выбита ПОСЛЕДНЯЯ волна, и реальным вызовом onAllWavesCleared —
    // нужна, чтобы успела доиграть анимация смерти финального врага (обычно босса, см. SceneFacade.cpp), а не
    // обрывалась на первом кадре экраном победы поверх. Мир НЕ ставится на паузу на это время (см. update()) —
    // тот же приём, что у PlayerDeathComponent, только с фактической задержкой самого onAllWavesCleared, а не
    // немедленным вызовом с доигрыванием анимации "под" уже открытым экраном. spawnStagger — необязательная пауза
    // (0 по умолчанию, как раньше — все бойцы волны спавнились в один кадр) МЕЖДУ появлением бойцов ОДНОЙ волны —
    // раньше волны "резко появлялись" всем составом разом; см. также SceneFacade.cpp, где на каждого
    // заспавненного бойца вдобавок навешивается SpawnFadeComponent (плавное проявление), staggering и fade вместе
    // и дают эффект "волна затекает на арену", а не "волна материализуется целиком".
    ArenaWaveComponent(std::vector<std::vector<WaveEnemySpec>> waves, std::vector<std::string> musicPaths,
        std::vector<sf::Vector2f> spawnPoints,
        std::function<GameObject*(const std::string& kind, sf::Vector2f position)> spawnEnemy,
        std::function<void()> onAllWavesCleared, sf::Time victoryDelay = sf::Time::Zero,
        sf::Time spawnStagger = sf::Time::Zero);

    void update(sf::Time dt) override;
    void reset() override;

    // Точки появления бойцов волны зависят от положения арены на карте (см. SceneFacade.cpp — арена сдвигается
    // при каждой пересборке формы уровня, offset считается от ширины подземелья, которая меняется). Компонент сам
    // не пересоздаётся при пересборке (на него держат ссылку многие обработчики меню) — вместо этого вызывающий
    // код просто обновляет список точек здесь, конструктор для этого не годится.
    void setSpawnPoints(std::vector<sf::Vector2f> spawnPoints)
    {
        m_spawnPoints = std::move(spawnPoints);
    }

    // Запускает волну 1 — идемпотентно повторный вызов, пока волны уже идут, ничего не делает (защита на случай,
    // если игрок как-то умудрится дёрнуть дверь ещё раз, пока она уже открыта — но у неё и так isOpen()-гейт).
    void start();

    // -1, если волны ещё не запущены (см. m_currentWave) — нужно "Сохранить"/"Продолжить"/"Загрузить сохранение"
    // в SceneFacade.cpp: раньше сохранение не помнило, что игрок вообще был на арене — позиция игрока (в мировых
    // координатах арены, далеко от подземелья) восстанавливалась, а границы камеры откатывались на подземелье
    // безусловно, и волна не перезапускалась — камера "залипала" на краю подземелья, игрок пропадал из виду (баг).
    int getCurrentWave() const
    {
        return m_currentWave;
    }

    // Запускает КОНКРЕТНУЮ волну (не обязательно первую) — нужно "Продолжить"/"Загрузить сохранение", чтобы
    // восстановить ту волну, на которой игрок сохранился (см. getCurrentWave() выше). Бойцы предыдущей сохранённой
    // волны не восстанавливаются поимённо (их точные HP не сохраняются, см. GameMemento) — волна перезапускается с
    // нуля, тем же способом, что и currentWaveCleared()-переход между волнами при обычной игре. index может быть
    // и РОВНО m_waves.size() — так помечается сейв, сделанный в паузу между тем, как выбита последняя волна, и
    // самим вызовом onAllWavesCleared (см. getCurrentWave()/finishAllWaves() ниже); больше — уже испорченный файл.
    void startAtWave(std::size_t index)
    {
        if (index > m_waves.size()) {
            return; // Защита от повреждённого/чужого файла сейва — m_waves[index] иначе читал бы за границей.
        }
        if (index == m_waves.size()) {
            // Раньше здесь просто ничего не происходило (тот же ранний return, что и выше) — тот путь ошибочно
            // ловил и этот легальный случай: сохранённая игра восстанавливалась на арене без единой волны и без
            // победы, сдвинуться дальше было некуда (критический баг, найден на ревью). Все волны уже выбиты —
            // ведём arenaWaves тем же путём, что и обычное завершение последней волны в update().
            finishAllWaves();
            return;
        }
        spawnWave(index);
    }

private:
    struct PendingSpawn {
        std::string kind;
        sf::Vector2f position;
    };

    void spawnWave(std::size_t index);
    // Общий хвост для обычного завершения последней волны (update()) и восстановления сейва, сделанного уже после
    // него (startAtWave() выше) — не дёргает onAllWavesCleared синхронно, а переводит компонент в то же состояние
    // ожидания victoryDelay, что и в реальной игре: реальный вызов случится на одном из следующих update(). Это
    // важно именно для загрузки — вызывающий код (SceneFacade::loadFromMemento) сбрасывает isVictory()/isPaused()
    // СРАЗУ ПОСЛЕ применения сейва тем же кадром; синхронный вызов onAllWavesCleared отсюда тут же оказался бы
    // затёрт этим сбросом.
    void finishAllWaves();
    // true, только когда и все живые бойцы волны мертвы, И очередь ещё не заспавненных бойцов той же волны пуста
    // (см. m_pendingSpawns) — без второго условия волна могла бы засчитаться выбитой ДО того, как весь её состав
    // вообще появился на арене (например, первые несколько заспавненных бойцов быстро погибли, пока остальные ещё
    // ждут своей очереди в очереди спавна).
    bool currentWaveCleared() const;

    std::vector<std::vector<WaveEnemySpec>> m_waves;
    std::vector<std::string> m_musicPaths;
    std::vector<sf::Vector2f> m_spawnPoints;
    std::function<GameObject*(const std::string&, sf::Vector2f)> m_spawnEnemy;
    std::function<void()> m_onAllWavesCleared;

    int m_currentWave = -1; // -1 — ещё не запущено.
    std::vector<GameObject*> m_currentWaveEnemies;

    sf::Time m_victoryDelay;
    bool m_victoryPending = false;
    sf::Time m_victoryDelayRemaining;

    sf::Time m_spawnStagger;
    std::vector<PendingSpawn> m_pendingSpawns;
    sf::Time m_spawnStaggerRemaining;
};
