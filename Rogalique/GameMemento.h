#pragma once
#include <SFML/System/Vector2.hpp>
#include <string>
#include <vector>

// Паттерн Хранитель.
class GameMemento {
public:
    // Один слот мешка на диске — itemId пустой значит слот пуст (count тогда не важен). Ссылается на предмет по
    // id (см. ItemDefinition::id/findItemDefinition), не по указателю — тот из статического реестра ItemDefinition.cpp
    // валиден только в рамках одного запуска игры, для файла нужен стабильный текстовый идентификатор.
    struct BagSlotSave {
        std::string itemId;
        int count = 0;
    };

    // Один слот экипировки — category это ItemCategory, приведённый к int (см. ItemDefinition.h); itemId пустой
    // значит категория ничем не занята. durability — оставшиеся заряды прочности (0 у Ring/Neck/Weapon и вообще
    // не ломающейся брони, см. InventoryComponent).
    struct EquipSlotSave {
        int category = 0;
        std::string itemId;
        int durability = 0;
    };

    // Один "лишний" бот сверх пяти именных (см. SceneFacade.cpp — extraEnemies/extraEnemyPositions, туда садятся
    // и переполнение EnemySlot, и повторяющиеся FixedEnemy одного kind). Индекс в массиве — тот же порядок, в
    // котором SceneFacade создаёт extraEnemies из LevelContent::extraEnemyPositions; тот порядок стабилен при
    // одном и том же сиде (см. hasLevelSeed), так что просто сопоставляем по индексу, отдельный id не нужен.
    struct ExtraEnemySave {
        sf::Vector2f position;
        int hp = 0;
    };

    // Сохраняет снимок в текстовый файл. Возвращает false при ошибке записи.
    bool save(const std::string& filePath) const;

    // Загружает снимок из файла. Возвращает false, если файла нет или он повреждён outMemento в этом случае не трогается, вызывающий код продолжает работать с тем, что было до вызова.
    static bool load(const std::string& filePath, GameMemento& outMemento);

    sf::Vector2f getPlayerPosition() const
    {
        return m_playerPosition;
    }
    int getPlayerHp() const
    {
        return m_playerHp;
    }
    sf::Vector2f getEnemyPosition() const
    {
        return m_enemyPosition;
    }
    int getEnemyHp() const
    {
        return m_enemyHp;
    }

    // false у файлов, сохранённых до появления Soldier/Slime — тогда вызывающий код сам решает, что делать
    // (например, вернуть его на точку спавна), а не берёт мусорные позицию/HP.
    bool hasSoldierData() const
    {
        return m_soldier.hasData;
    }
    sf::Vector2f getSoldierPosition() const
    {
        return m_soldier.position;
    }
    int getSoldierHp() const
    {
        return m_soldier.hp;
    }

    // Slime без номера — первая расцветка (Slime1), появилась раньше двух остальных (см. Slime.cpp skin).
    bool hasSlimeData() const
    {
        return m_slime1.hasData;
    }
    sf::Vector2f getSlimePosition() const
    {
        return m_slime1.position;
    }
    int getSlimeHp() const
    {
        return m_slime1.hp;
    }

    bool hasSlime2Data() const
    {
        return m_slime2.hasData;
    }
    sf::Vector2f getSlime2Position() const
    {
        return m_slime2.position;
    }
    int getSlime2Hp() const
    {
        return m_slime2.hp;
    }

    bool hasSlime3Data() const
    {
        return m_slime3.hasData;
    }
    sf::Vector2f getSlime3Position() const
    {
        return m_slime3.position;
    }
    int getSlime3Hp() const
    {
        return m_slime3.hp;
    }

    // false у файлов, сохранённых до появления инвентаря/боезапаса — тогда вызывающий код оставляет то, что уже
    // есть у только что созданной сцены (пустой мешок, полный боезапас), а не затирает мусором.
    bool hasInventoryData() const
    {
        return m_inventory.hasData;
    }
    const std::vector<BagSlotSave>& getBagSlots() const
    {
        return m_inventory.bag;
    }
    const std::vector<EquipSlotSave>& getEquipSlots() const
    {
        return m_inventory.equipment;
    }
    int getSoldierArrows() const
    {
        return m_inventory.soldierArrows;
    }
    int getSlime3ShotsFired() const
    {
        return m_inventory.slime3ShotsFired;
    }
    // По индексу в том же порядке, в котором SceneFacade расставляет ItemPickup на карте (см. itemPickups) — true,
    // если этот конкретный предмет на момент сохранения уже был подобран (и не должен снова появиться на карте
    // при загрузке поверх того же предмета, уже лежащего в мешке/надетого).
    const std::vector<int>& getCollectedPickups() const
    {
        return m_inventory.collectedPickups;
    }
    // false у сейвов, записанных до появления сида содержимого уровня (см. LevelContent/buildLevelContent в
    // SceneFacade) — тогда вызывающий код просто оставляет то содержимое (расстановку ботов/предметов), что уже
    // выбрано для этого запуска процесса, вместо мусора. Это сид только содержимого — какой бот/предмет в каком
    // из заготовленных слотов оказался; сид формы самой планировки (какие комнаты/чанки и в каком порядке,
    // см. SceneFacade::rebuildLevelGeometry) — отдельное поле, см. hasLevelShapeSeed()/setLevelShapeSeed() ниже.
    bool hasLevelSeed() const
    {
        return m_inventory.hasLevelSeed;
    }
    unsigned getLevelSeed() const
    {
        return m_inventory.levelSeed;
    }
    void setLevelSeed(unsigned seed)
    {
        m_inventory.levelSeed = seed;
        m_inventory.hasLevelSeed = true;
    }

    // false у сейвов, записанных до появления "лишних" ботов (см. ExtraEnemySave выше) — тогда вызывающий код
    // просто сбрасывает их на спавн (как и раньше), а не читает мусор.
    bool hasExtraEnemiesData() const
    {
        return m_inventory.hasExtraEnemies;
    }
    const std::vector<ExtraEnemySave>& getExtraEnemies() const
    {
        return m_inventory.extraEnemies;
    }
    void setExtraEnemies(std::vector<ExtraEnemySave> extras)
    {
        m_inventory.extraEnemies = std::move(extras);
        m_inventory.hasExtraEnemies = true;
    }

    // Сид ФОРМЫ уровня (какие чанки собрались в подземелье, см. ChunkAssembler/assembleChunkedLevel) — отдельный
    // от levelSeed выше (тот только про рассадку ботов/предметов ПО УЖЕ ГОТОВЫМ слотам). Раньше форма собиралась
    // ровно один раз за весь процесс и в сейв не попадала вовсе — с появлением пересборки формы при каждом
    // "Начать" (см. SceneFacade.cpp) без этого поля "Продолжить"/"Загрузить сохранение" применяли бы сохранённые
    // координаты актёров к СЛУЧАЙНО ДРУГОЙ, уже пересобранной форме — персонажи оказывались бы внутри стен или в
    // чужих комнатах. false у сейвов старее этого поля — тогда просто оставляем ту форму, что уже собрана в
    // текущем запуске процесса (как было раньше).
    bool hasLevelShapeSeed() const
    {
        return m_inventory.hasLevelShapeSeed;
    }
    unsigned getLevelShapeSeed() const
    {
        return m_inventory.levelShapeSeed;
    }
    void setLevelShapeSeed(unsigned seed)
    {
        m_inventory.levelShapeSeed = seed;
        m_inventory.hasLevelShapeSeed = true;
    }

    // Игрок был на арене волн в момент сохранения (см. ArenaWaveComponent::getCurrentWave — -1, если волны ещё не
    // запущены). false у сейвов старее этого поля — тогда вызывающий код просто не трогает волны/границы камеры
    // арены, как и раньше. arenaWave, равный числу волн (не индекс существующей волны, а ровно за её пределами) —
    // отдельный легальный случай: последняя волна уже выбита, но игрок успел сохраниться в паузу ДО того, как
    // отыграл victoryDelay и показался экран победы (см. ArenaWaveComponent::finishAllWaves/startAtWave) — не
    // "разгар боя", а ожидание победы.
    bool hasArenaWaveData() const
    {
        return m_inventory.hasArenaWave;
    }
    int getArenaWave() const
    {
        return m_inventory.arenaWave;
    }
    void setArenaWave(int wave)
    {
        m_inventory.arenaWave = wave;
        m_inventory.hasArenaWave = true;
    }

    // Максимум HP игрока (см. KillStreakComponent — растёт за убийства ботов в подземелье) и счётчик убийств,
    // накопленный к текущей "пятёрке" (0..4, обнуляется до 0 каждый раз, когда даёт +1 к максимуму). false у
    // сейвов старее этой награды — тогда вызывающий код просто оставляет базовый PLAYER_MAX_HP и нулевой счётчик,
    // как было раньше. Последнее звено цепочки, после arenaWave.
    bool hasKillStreakData() const
    {
        return m_inventory.hasKillStreak;
    }
    int getPlayerMaxHp() const
    {
        return m_inventory.playerMaxHp;
    }
    int getDungeonKillStreak() const
    {
        return m_inventory.dungeonKillStreak;
    }
    void setKillStreakData(int playerMaxHp, int dungeonKillStreak)
    {
        m_inventory.playerMaxHp = playerMaxHp;
        m_inventory.dungeonKillStreak = dungeonKillStreak;
        m_inventory.hasKillStreak = true;
    }

    // Секундомер забега на момент сохранения (см. GameTimerComponent) — false у сейвов старее этого поля, тогда
    // вызывающий код просто сбрасывает таймер на ноль, как было раньше. Последнее звено цепочки, после killStreak.
    // Без этого поля "Продолжить"/"Загрузить сохранение" обнуляли бы секундомер безусловно — пересохранение прямо
    // перед финишем занижало бы честное время в таблице лидеров (баг, найден при проверке сохранения на абьюзы).
    bool hasElapsedTimeData() const
    {
        return m_inventory.hasElapsedTime;
    }
    float getElapsedSeconds() const
    {
        return m_inventory.elapsedSeconds;
    }
    void setElapsedTime(float elapsedSeconds)
    {
        m_inventory.elapsedSeconds = elapsedSeconds;
        m_inventory.hasElapsedTime = true;
    }

    // Сундуки (см. Chest/ChestComponent) — по индексу в том же порядке, в котором SceneFacade обходит их через
    // getComponentsInChildren<ChestComponent>() (стабилен, пока не пересобралась форма уровня — см.
    // hasLevelShapeSeed, тот сид применяется раньше этого поля при загрузке). true, если конкретный сундук на
    // момент сохранения уже был открыт. Раньше состояние сундука сознательно не сохранялось ("часть планировки
    // уровня, а не прогресс"), но предмет из сундука УЖЕ попадает в сохраняемый инвентарь — без этого поля сундук
    // при загрузке открывался бы заново и отдавал тот же предмет второй раз (баг-дубликат, найден при аудите
    // сохранений). false у сейвов старее этого поля — тогда вызывающий код просто оставляет все сундуки закрытыми,
    // как было раньше. Последнее звено цепочки, после elapsedTime.
    bool hasOpenedChestsData() const
    {
        return m_inventory.hasOpenedChests;
    }
    const std::vector<int>& getOpenedChests() const
    {
        return m_inventory.openedChests;
    }
    void setOpenedChests(std::vector<int> openedChests)
    {
        m_inventory.openedChests = std::move(openedChests);
        m_inventory.hasOpenedChests = true;
    }

    void setPlayerPosition(sf::Vector2f position)
    {
        m_playerPosition = position;
    }
    void setPlayerHp(int hp)
    {
        m_playerHp = hp;
    }
    void setEnemyPosition(sf::Vector2f position)
    {
        m_enemyPosition = position;
    }
    void setEnemyHp(int hp)
    {
        m_enemyHp = hp;
    }
    void setSoldierPosition(sf::Vector2f position)
    {
        m_soldier.position = position;
        m_soldier.hasData = true;
    }
    void setSoldierHp(int hp)
    {
        m_soldier.hp = hp;
        m_soldier.hasData = true;
    }
    void setSlimePosition(sf::Vector2f position)
    {
        m_slime1.position = position;
        m_slime1.hasData = true;
    }
    void setSlimeHp(int hp)
    {
        m_slime1.hp = hp;
        m_slime1.hasData = true;
    }
    void setSlime2Position(sf::Vector2f position)
    {
        m_slime2.position = position;
        m_slime2.hasData = true;
    }
    void setSlime2Hp(int hp)
    {
        m_slime2.hp = hp;
        m_slime2.hasData = true;
    }
    void setSlime3Position(sf::Vector2f position)
    {
        m_slime3.position = position;
        m_slime3.hasData = true;
    }
    void setSlime3Hp(int hp)
    {
        m_slime3.hp = hp;
        m_slime3.hasData = true;
    }

    // Все сразу — записываются одним блоком (см. save()/load()), нет смысла выставлять их по отдельности.
    void setInventoryData(std::vector<BagSlotSave> bag, std::vector<EquipSlotSave> equipment, int soldierArrows,
        int slime3ShotsFired, std::vector<int> collectedPickups)
    {
        m_inventory.bag = std::move(bag);
        m_inventory.equipment = std::move(equipment);
        m_inventory.soldierArrows = soldierArrows;
        m_inventory.slime3ShotsFired = slime3ShotsFired;
        m_inventory.collectedPickups = std::move(collectedPickups);
        m_inventory.hasData = true;
    }

private:
    // hasData=false — необязательная запись (существо появилось в более поздней версии сейва, см. save()/load():
    // читаются/пишутся строго по порядку объявления ниже, первая отсутствующая обрывает цепочку последующих).
    struct OptionalEntitySave {
        sf::Vector2f position;
        int hp = 0;
        bool hasData = false;
    };

    struct OptionalInventorySave {
        std::vector<BagSlotSave> bag;
        std::vector<EquipSlotSave> equipment;
        int soldierArrows = 0;
        int slime3ShotsFired = 0;
        std::vector<int> collectedPickups;
        bool hasData = false;
        // Отдельный от hasData флаг — пишется/читается ПОСЛЕ collectedPickups как самостоятельный необязательный
        // хвост (см. save()/load()): сейв мог быть создан этой же сессией до появления этого поля, тогда bag/
        // equipment/collectedPickups всё равно валидны, просто содержимое уровня неизвестно — не повод откатывать всё.
        unsigned levelSeed = 0;
        bool hasLevelSeed = false;
        // Ещё один самостоятельный необязательный хвост (после levelSeed) — та же логика, что у него самого:
        // сейв мог быть записан этой же сессией до появления "лишних" ботов.
        std::vector<ExtraEnemySave> extraEnemies;
        bool hasExtraEnemies = false;
        // См. hasLevelShapeSeed()/setLevelShapeSeed() выше.
        unsigned levelShapeSeed = 0;
        bool hasLevelShapeSeed = false;
        // Последнее звено цепочки (после levelShapeSeed) — см. hasArenaWaveData()/setArenaWave() выше.
        int arenaWave = -1;
        bool hasArenaWave = false;
        // Звено после arenaWave — см. hasKillStreakData()/setKillStreakData() выше.
        int playerMaxHp = 0;
        int dungeonKillStreak = 0;
        bool hasKillStreak = false;
        // Звено после killStreak — см. hasElapsedTimeData()/setElapsedTime() выше.
        float elapsedSeconds = 0.f;
        bool hasElapsedTime = false;
        // Самое новое звено (после elapsedTime) — см. hasOpenedChestsData()/setOpenedChests() выше.
        std::vector<int> openedChests;
        bool hasOpenedChests = false;
    };

    sf::Vector2f m_playerPosition;
    int m_playerHp = 0;
    sf::Vector2f m_enemyPosition;
    int m_enemyHp = 0;
    OptionalEntitySave m_soldier;
    OptionalEntitySave m_slime1;
    OptionalEntitySave m_slime2;
    OptionalEntitySave m_slime3;
    // Последнее звено цепочки (после slime3) — сейвы старее инвентаря просто не доходят досюда при чтении.
    OptionalInventorySave m_inventory;
};
