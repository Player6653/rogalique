#include "SceneFacade.h"
#include "ArenaWaveComponent.h"
#include "ArmorBadgeComponent.h"
#include "ArrowCrate.h"
#include "AudioSystem.h"
#include "Boss.h"
#include "BossTeleportComponent.h"
#include "CameraComponent.h"
#include "HealthChangeFeedbackComponent.h"
#include "ParticleSystem.h"
#include "ParticleSystemComponent.h"
#include "Chest.h"
#include "ChestComponent.h"
#include "ChunkAssembler.h"
#include "ColliderComponent.h"
#include "ConsoleSink.h"
#include "CreditsOverlayComponent.h"
#include "DisplaySettings.h"
#include "Door.h"
#include "DoorComponent.h"
#include "Enemy.h"
#include "Engine.h"
#include "FileSink.h"
#include "GameMemento.h"
#include "Location.h"
#include "GameTimerComponent.h"
#include "GameWorld.h"
#include "HealthBarComponent.h"
#include "FractionBarComponent.h"
#include "HealthComponent.h"
#include "HudTextComponent.h"
#include "IAnimatedActor.h"
#include "ToastNotificationComponent.h"
#include "InputComponent.h"
#include "InventoryComponent.h"
#include "InventoryOverlayComponent.h"
#include "ItemDefinition.h"
#include "ItemPickup.h"
#include "ItemPickupComponent.h"
#include "KillStreakComponent.h"
#include "Leaderboard.h"
#include "Log.h"
#include "LowHealthScreenFlashComponent.h"
#include "MenuOverlayComponent.h"
#include "NameEntryOverlayComponent.h"
#include "PauseToggleComponent.h"
#include "Pit.h"
#include "Player.h"
#include "PlayerAttackComponent.h"
#include "PlayerDeathComponent.h"
#include "RenderSystem.h"
#include "SavePaths.h"
#include "ScreenFadeComponent.h"
#include "SettingsOverlayComponent.h"
#include "SpawnFadeComponent.h"
#include "Slime.h"
#include "SlimeShotLimitComponent.h"
#include "Soldier.h"
#include "SoldierAmmoComponent.h"
#include "SpriteComponent.h"
#include "TiledBackgroundComponent.h"
#include "TiledLevel.h"
#include "TransientComponent.h"
#include "Trap.h"
#include "VampireSpawnMinion.h"
#include "WeaponComponent.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
    constexpr float TILE_SIZE = 48.f;

    // См. использование у ArenaWaveComponent::victoryDelay ниже — экран победы ждёт, пока доиграет анимация
    // смерти босса (Boss.cpp: DEATH_FRAME_COUNT=57 кадров по 0.04с ≈ 2.28с), а не появляется на первом кадре.
    const sf::Time BOSS_VICTORY_DELAY = sf::seconds(2.4f);

    // Порядок слотов экипировки в оверлее инвентаря (см. InventoryOverlayComponent) — рамка каждого слота
    // называется по этому же имени (Resources/GUI/Frame_<имя>.png). Индекс в этом массиве == индекс слота,
    // который InventoryOverlayComponent возвращает в getEquipSlot/onEquipSlotClicked.
    const ItemCategory EQUIP_CATEGORY_ORDER[] = {ItemCategory::Weapon, ItemCategory::Head, ItemCategory::Chest,
        ItemCategory::Boots, ItemCategory::Ring, ItemCategory::Neck, ItemCategory::Shield, ItemCategory::Pants};
    const char* EQUIP_FRAME_NAMES[] = {"Weapon", "Head", "Chest", "Boots", "Ring", "Neck", "Shield", "Pants"};

    // Объект FixedEnemy в Tiled (свойство kind — см. ENEMY_KIND_NAMES ниже) — конкретный бот в конкретной точке,
    // без рандома, в отличие от EnemySlot (просто место, которое заполняется случайно любым из пяти видов, см.
    // buildLevelContent). Объект FixedItem (свойства itemId/count/requiresInteract) — та же идея для предметов,
    // альтернатива ItemSlot.
    struct FixedEnemySpec {
        std::string kind;
        sf::Vector2f position;
    };
    struct FixedItemSpec {
        std::string itemId;
        int count;
        bool requiresInteract;
        sf::Vector2f position;
    };

    // Порядок совпадает с тем, в каком боты создаются в run() (0=Enemy/orc, 1=Soldier, 2..4=Slime1-3) — строка в
    // FixedEnemy::kind должна совпадать буквально с одной из этих пяти.
    constexpr const char* ENEMY_KIND_NAMES[5] = {"orc", "soldier", "slime1", "slime2", "slime3"};

    // Пять именованных ботов (Enemy/Soldier/Slime1-3, см. ENEMY_KIND_NAMES) — у каждого либо своя точка из
    // FixedEnemy (пришла как есть, без рандома), либо, если для этого вида в fixedEnemies ничего нет, следующая
    // случайная позиция из перемешанных enemySlots. Слоты, оставшиеся невостребованными сверх этих пяти (например,
    // игрок поставил 8 EnemySlot, а именных видов всего 5) — extraEnemyPositions, см. run(): туда садятся
    // дополнительные простые боты (слаймы), чтобы расставленные слоты не пропадали впустую.
    struct LevelContent {
        sf::Vector2f enemyPositions[5];
        std::vector<sf::Vector2f> extraEnemyPositions;
        // Тот же индекс, что и в extraEnemyPositions выше — вид конкретного "лишнего" бота, если он известен
        // (пришёл из дубликата FixedEnemy — см. buildLevelContent), иначе пустая строка (анонимное переполнение
        // EnemySlot — какой слайм туда сядет, решает цикл в run(), как и раньше). Раньше дубликаты
        // FixedEnemy всегда превращались в случайного слайма, даже если в Tiled стоял orc/soldier.
        std::vector<std::string> extraEnemyKinds;
        std::vector<sf::Vector2f> itemPositions;
    };

    // Конкретный экземпляр вида по строке kind (см. ENEMY_KIND_NAMES) — теми же параметрами (размер/скорость/
    // радиус обнаружения), что и у именных ботов в run(). nullptr при пустом/незнакомом kind — вызывающий код сам
    // решает, что делать (например, откатиться на случайного слайма, как раньше). Используется и для дубликатов
    // FixedEnemy (переполнение сверх пяти именных видов), и для волн арены (см. ArenaWaveComponent) — единое место,
    // чтобы параметры конкретного вида не разъезжались по разным кускам кода.
    std::unique_ptr<GameObject> makeEnemyInstance(const std::string& kind, sf::Vector2f position, GameObject* slimeSplitParent,
        const SlimeConfig& slime1Config, const SlimeConfig& slime2Config, const SlimeConfig& slime3Config)
    {
        if (kind == "orc") {
            return std::make_unique<Enemy>(position, sf::Vector2f(32.f, 32.f), 140.f, 220.f);
        }
        if (kind == "soldier") {
            return std::make_unique<Soldier>(position, sf::Vector2f(32.f, 32.f), 120.f, 280.f);
        }
        SlimeConfig cfg;
        if (kind == "slime1") {
            cfg = slime1Config;
        } else if (kind == "slime2") {
            cfg = slime2Config;
        } else if (kind == "slime3") {
            cfg = slime3Config;
        } else {
            return nullptr;
        }
        cfg.childSpawnParent = slimeSplitParent;
        return std::make_unique<Slime>(position, sf::Vector2f(28.f, 28.f), 90.f, 220.f, cfg);
    }

    // Форма и число слотов — то, что расставил игрок в Tiled (enemySlots/itemSlots/fixedEnemies, уже переведены в
    // мировые координаты вызывающим кодом) — не меняется от сида. Реиграбельность здесь: какой из ЕЩЁ НЕ
    // закреплённых FixedEnemy пятью видами ботов попадёт в какой оставшийся слот, и в каком слоте окажется какой
    // предмет. Каждый вызов с новым seed — новая случайная расстановка (см. кнопку "Начать" в run(), которая
    // зовёт это при каждом нажатии, и "Продолжить"/"Загрузить сохранение", которые зовут с сидом из сейва, чтобы
    // восстановить именно ту расстановку, что была при сохранении). FixedEnemy/FixedItem-позиции сидом не
    // затрагиваются вовсе — они не часть перемешивания.
    LevelContent buildLevelContent(unsigned seed, std::vector<sf::Vector2f> enemySlots,
        const std::vector<FixedEnemySpec>& fixedEnemies, std::vector<sf::Vector2f> itemSlots)
    {
        std::mt19937 rng(seed);
        std::shuffle(enemySlots.begin(), enemySlots.end(), rng);
        std::shuffle(itemSlots.begin(), itemSlots.end(), rng);

        LevelContent content;
        bool filled[5] = {false, false, false, false, false};
        for (const FixedEnemySpec& fixed : fixedEnemies) {
            bool matchedKind = false;
            for (int k = 0; k < 5; ++k) {
                if (fixed.kind == ENEMY_KIND_NAMES[k]) {
                    matchedKind = true;
                    if (filled[k]) {
                        // Именной вид уже занят другим FixedEnemy того же kind (например, "orc" стоит в двух
                        // разных комнатах пула, а в сборку могли попасть обе сразу) — раньше второй экземпляр
                        // просто пропадал целиком (был баг — не все боты спавнились). Теперь не теряем —
                        // садится как обычный "лишний" бот (см. extraEnemyPositions/run() — цикл по Slime1-3),
                        // просто фиксированной точкой, а не случайным EnemySlot.
                        LOG_WARN("SceneFacade: несколько FixedEnemy с kind=\"" + fixed.kind
                                 + "\" — именной уже занят, этот экземпляр спавнится дополнительной копией того же вида");
                        content.extraEnemyPositions.push_back(fixed.position);
                        content.extraEnemyKinds.push_back(fixed.kind);
                    } else {
                        content.enemyPositions[k] = fixed.position;
                        filled[k] = true;
                    }
                    break;
                }
            }
            if (!matchedKind) {
                LOG_WARN("SceneFacade: у объекта FixedEnemy неизвестный kind=\"" + fixed.kind
                         + "\" (ожидался один из orc/soldier/slime1/slime2/slime3)");
            }
        }
        std::size_t nextSlot = 0;
        for (int k = 0; k < 5; ++k) {
            if (filled[k]) {
                continue;
            }
            if (nextSlot < enemySlots.size()) {
                content.enemyPositions[k] = enemySlots[nextSlot++];
                filled[k] = true;
            }
        }
        for (; nextSlot < enemySlots.size(); ++nextSlot) {
            content.extraEnemyPositions.push_back(enemySlots[nextSlot]);
            content.extraEnemyKinds.push_back(std::string());
        }
        // Второй проход — если какому-то из пяти именных видов всё ещё не хватило прямого EnemySlot (иначе бот
        // спавнился бы в (0,0)), забираем позицию у уже собранных "лишних" (extraEnemyPositions выше —
        // и переполнение EnemySlot, и дубли FixedEnemy одного kind, см. цикл по fixedEnemies выше): она изначально
        // предназначалась для другого вида/для "лишнего" бота, но реальная мировая позиция ничем не хуже нуля в
        // углу карты. Раз общее число позиций (enemySlots+fixedEnemies) гарантированно >= 5 (см. проверку в
        // run()), а сюда мы попадаем только если каких-то из пяти именных не хватило напрямую — "лишних" на эту
        // недостачу гарантированно хватает: extraEnemyPositions.size() == totalPositions - filledNamedCount >=
        // 5 - filledNamedCount == сколько тут ещё не заполнено.
        for (int k = 0; k < 5; ++k) {
            if (filled[k]) {
                continue;
            }
            if (!content.extraEnemyPositions.empty()) {
                content.enemyPositions[k] = content.extraEnemyPositions.back();
                content.extraEnemyPositions.pop_back();
                content.extraEnemyKinds.pop_back();
                filled[k] = true;
            } else {
                // Арифметически недостижимо (см. комментарий выше) — оставлено на случай, если инвариант всё же
                // где-то нарушится, лучше явная позиция (0,0) с ошибкой в логе, чем тихий баг.
                LOG_ERROR("SceneFacade: не хватило EnemySlot для \"" + std::string(ENEMY_KIND_NAMES[k]) + "\"");
            }
        }
        content.itemPositions = std::move(itemSlots);
        return content;
    }

    // Полный сброс на точку спавна — общий хвост всех "В главное меню"/"Начать заново" пунктов меню ниже, один
    // на каждого враждебного актёра (Enemy/Soldier/Slime1-3/ArrowCrate), не входящего в сохранение целиком.
    void resetToSpawn(GameObject& object, sf::Vector2f spawnPosition)
    {
        object.resetComponents();
        object.setPosition(spawnPosition);
    }

    // Длительность плавного проявления только что заспавненного бойца волны арены (см. spawnEnemy у
    // ArenaWaveComponent ниже) и пауза между появлением бойцов ОДНОЙ волны — раньше все они появлялись мгновенно и
    // всем составом разом.
    const sf::Time ARENA_SPAWN_FADE_DURATION = sf::seconds(0.4f);
    const sf::Time ARENA_SPAWN_STAGGER = sf::seconds(0.3f);

    // Вешает SpawnFadeComponent (Engine) на тело+тень только что заспавненного бойца — общий хвост для боевой и
    // обычной веток spawnEnemy у ArenaWaveComponent ниже, не завязан на конкретный вид существа (любой актёр,
    // реализующий IAnimatedActor — Enemy/Soldier/Slime/Boss/VampireSpawnMinion, все они его уже реализуют).
    void attachSpawnFade(GameObject* actor)
    {
        if (!actor) {
            return;
        }
        if (auto* animated = dynamic_cast<IAnimatedActor*>(actor)) {
            actor->addComponent<SpawnFadeComponent>(animated->getBodySprite(), ARENA_SPAWN_FADE_DURATION);
            actor->addComponent<SpawnFadeComponent>(animated->getShadowSprite(), ARENA_SPAWN_FADE_DURATION);
        }
    }

    // Либо ставит позицию/HP из сохранения (если для этого актёра там есть данные — см. GameMemento::hasXData()),
    // либо, если сейв старее появления этого актёра, полный сброс на точку спавна (см. resetToSpawn) — общий хвост
    // "Продолжить"/"Загрузить сохранение" ниже для необязательных записей (Soldier, Slime1-3).
    void applyOptionalMemento(GameObject& object, HealthComponent* health, sf::Vector2f spawnPosition, bool hasData,
        sf::Vector2f savedPosition, int savedHp)
    {
        if (hasData) {
            object.setPosition(savedPosition);
            if (health) {
                health->setHp(savedHp);
            }
        } else {
            resetToSpawn(object, spawnPosition);
        }
    }

    // Тот же принцип, что у applyOptionalMemento выше, но для всего вектора "лишних" ботов сразу (см.
    // extraEnemies/extraEnemyPositions в run()) — раньше их не было в сейве вовсе (баг: убитые "лишние" боты
    // оживали заново после "Продолжить"/"Загрузить сохранение", а те, что просто отошли от точки спавна во время
    // игры, не возвращались на место при загрузке). По индексу by-index (см. GameMemento::ExtraEnemySave) — если
    // в сейве для данного индекса есть запись, ставим позицию/HP из неё; если нет (сейв старее, либо сейчас ботов
    // больше, чем было при сохранении — игрок добавил ещё в Tiled) — обычный resetToSpawn на текущую спавн-точку.
    void applyExtraEnemiesMemento(const std::vector<GameObject*>& extraEnemies,
        const std::vector<sf::Vector2f>& extraEnemyPositions, const GameMemento& memento)
    {
        const std::vector<GameMemento::ExtraEnemySave>& saved = memento.getExtraEnemies();
        bool hasSaved = memento.hasExtraEnemiesData();
        for (std::size_t i = 0; i < extraEnemies.size(); ++i) {
            if (hasSaved && i < saved.size()) {
                extraEnemies[i]->setPosition(saved[i].position);
                if (auto* health = extraEnemies[i]->getComponent<HealthComponent>()) {
                    health->setHp(saved[i].hp);
                }
            } else if (i < extraEnemyPositions.size()) {
                resetToSpawn(*extraEnemies[i], extraEnemyPositions[i]);
            }
        }
    }

    // Восстанавливает бой на арене волн, если сейв был сделан прямо во время него (см.
    // ArenaWaveComponent::getCurrentWave/startAtWave и GameMemento::hasArenaWaveData) — раньше "Продолжить"/
    // "Загрузить сохранение" ОБА безусловно откатывали волны и границы камеры на подземелье: игрок оказывался в
    // мировых координатах арены (далеко от подземелья), а камера была ограничена границами подземелья и переставала
    // следовать за ним — персонаж пропадал с экрана (был баг). Бойцы
    // сохранённой волны поимённо не восстанавливаются (их HP не пишутся в сейв, см. GameMemento) — волна просто
    // перезапускается с нуля, тем же способом, что и обычный переход между волнами.
    // Возвращает true, если сейв восстановлен прямо посреди боя на арене — тогда startAtWave() ниже уже сама
    // включила музыку нужной волны (см. ArenaWaveComponent::spawnWave), и вызывающий код НЕ должен следом
    // безусловно переключать её на theme.wav (раньше именно так и стирало музыку волны обратно на подземелье).
    bool applyArenaWaveMemento(ArenaWaveComponent& arenaWaves, CameraComponent* camera, sf::FloatRect dungeonCameraBounds,
        sf::FloatRect arenaCameraBounds, const GameMemento& memento)
    {
        arenaWaves.reset();
        if (memento.hasArenaWaveData() && memento.getArenaWave() >= 0) {
            if (camera) {
                camera->setBounds(arenaCameraBounds);
            }
            arenaWaves.startAtWave(static_cast<std::size_t>(memento.getArenaWave()));
            return true;
        }
        if (camera) {
            camera->setBounds(dungeonCameraBounds);
        }
        return false;
    }

    // Сундуки и дверь (см. Chest/ChestComponent, Door/DoorComponent) живут в levelContainer, не в actors/
    // itemPickups — их состояние "открыт"/E-зажата сбрасывается отдельно от resetComponents() выше, этим общим
    // хвостом, а не парой циклов, продублированных в каждом из шести мест ниже ("Продолжить"/"Загрузить
    // сохранение"/оба "В главное меню"/"Играть заново"), где раньше вручную писали один и тот же for по
    // ChestComponent и второй по DoorComponent.
    void resetInteractables()
    {
        for (ChestComponent* chest : GameWorld::instance().getRoot().getComponentsInChildren<ChestComponent>()) {
            chest->reset();
        }
        for (DoorComponent* door : GameWorld::instance().getRoot().getComponentsInChildren<DoorComponent>()) {
            door->reset();
        }
    }

    // См. resetInteractables() выше — тот же принцип, но для resyncInteract() (см. ItemPickupComponent::
    // resyncInteract() и комментарий у resyncPlayerInput в run(), зачем это вообще нужно после снятия паузы).
    void resyncInteractables()
    {
        for (ChestComponent* chest : GameWorld::instance().getRoot().getComponentsInChildren<ChestComponent>()) {
            chest->resyncInteract();
        }
        for (DoorComponent* door : GameWorld::instance().getRoot().getComponentsInChildren<DoorComponent>()) {
            door->resyncInteract();
        }
    }

    // Восстанавливает мешок+экипировку игрока, боезапас Soldier и счётчик выстрелов Slime3 из сейва — общий хвост
    // "Продолжить"/"Загрузить сохранение" ниже, симметричный applyOptionalMemento() выше. Без данных в сейве
    // (старый файл без инвентаря) просто ничего не трогает — сцена уже в чистом состоянии (пустой мешок, полный
    // боезапас) к этому моменту, вызывающий код сам об этом позаботился (resetComponents()/свежая сцена).
    void applyInventoryMemento(InventoryComponent* playerInventory, SoldierAmmoComponent* soldierAmmo,
        SlimeShotLimitComponent* slime3ShotLimit, const std::vector<GameObject*>& itemPickups, const GameMemento& memento)
    {
        if (!memento.hasInventoryData()) {
            return;
        }
        if (playerInventory) {
            playerInventory->reset();
            for (const GameMemento::BagSlotSave& slot : memento.getBagSlots()) {
                if (slot.itemId.empty()) {
                    continue;
                }
                const ItemDefinition* item = findItemDefinition(slot.itemId);
                if (item) {
                    playerInventory->addItem(*item, slot.count);
                }
            }
            for (const GameMemento::EquipSlotSave& slot : memento.getEquipSlots()) {
                if (slot.itemId.empty()) {
                    continue;
                }
                const ItemDefinition* item = findItemDefinition(slot.itemId);
                if (item) {
                    playerInventory->forceEquip(static_cast<ItemCategory>(slot.category), *item, slot.durability);
                }
            }
            playerInventory->recomputeEquipmentEffects();
        }
        if (soldierAmmo) {
            soldierAmmo->setArrows(memento.getSoldierArrows());
        }
        if (slime3ShotLimit) {
            slime3ShotLimit->setShotsFired(memento.getSlime3ShotsFired());
        }
        // По индексу совпадает с порядком spawnItemPickup() в SceneFacade — resetComponents() выше на всех
        // itemPickups (см. вызовы вокруг applyInventoryMemento) уже вернул их видимыми, здесь только те, что
        // сейв запомнил уже подобранными, снова прячем, чтобы не задвоить предмет, уже осевший в мешке/экипировке.
        const std::vector<int>& collected = memento.getCollectedPickups();
        for (std::size_t i = 0; i < collected.size() && i < itemPickups.size(); ++i) {
            if (!collected[i]) {
                continue;
            }
            auto* pickupComponent = itemPickups[i]->getComponent<ItemPickupComponent>();
            if (pickupComponent) {
                pickupComponent->setCollected(true);
            }
        }
    }

    // Варианты лимита кадров в секунду для настроек.
    constexpr unsigned FPS_LIMIT_OPTIONS[] = {30, 60, 90, 120, 144, 240, 280, 0};
    constexpr int FPS_LIMIT_OPTION_COUNT = 8;

    // Индекс варианта, ближайшего к текущему лимиту окна — на случай, если он был выставлен не через эти варианты (например, значение по умолчанию 0 при старте).
    int currentFpsLimitIndex()
    {
        unsigned current = RenderSystem::instance().getFramerateLimit();
        for (int i = 0; i < FPS_LIMIT_OPTION_COUNT; ++i) {
            if (FPS_LIMIT_OPTIONS[i] == current) {
                return i;
            }
        }
        return 0;
    }

    // Варианты разрешения окна для настроек — см. DisplaySettings.h, почему применяются со следующего запуска, а
    // не сразу. 1248x768 (индекс 0) — прежнее жёстко зашитое разрешение, оставлено первым вариантом намеренно.
    struct ResolutionOption {
        int width;
        int height;
    };
    constexpr ResolutionOption RESOLUTION_OPTIONS[] = {{1248, 768}, {1600, 900}, {1920, 1080}, {2560, 1440}};
    constexpr int RESOLUTION_OPTION_COUNT = 4;

    int currentResolutionIndex(const DisplaySettings& settings)
    {
        for (int i = 0; i < RESOLUTION_OPTION_COUNT; ++i) {
            if (RESOLUTION_OPTIONS[i].width == settings.width && RESOLUTION_OPTIONS[i].height == settings.height) {
                return i;
            }
        }
        return 0;
    }

    // "M:SS" — общий формат для HUD-таймера (см. GameTimerComponent) и экрана победы/таблицы лидеров, без ведущих
    // нулей у минут (партия дольше 9 минут не ожидается, но на всякий случай не обрежет).
    std::string formatElapsedTime(sf::Time elapsed)
    {
        int totalSeconds = static_cast<int>(elapsed.asSeconds());
        int minutes = totalSeconds / 60;
        int seconds = totalSeconds % 60;
        std::string secondsStr = std::to_string(seconds);
        if (secondsStr.size() < 2) {
            secondsStr = "0" + secondsStr;
        }
        return std::to_string(minutes) + ":" + secondsStr;
    }
} // namespace

void SceneFacade::run()
{
    // Общий канал "global" пишет и в консоль, и в файл настраивается один раз здесь, дальше LOG_INFO/WARN/ERROR из любого места движка или игры доходят в оба места без дополнительной настройки.
    Logger& globalLogger = LoggerRegistry::getInstance().getLogger("global");
    globalLogger.addSink(std::make_unique<ConsoleSink>());
    globalLogger.addSink(std::make_unique<FileSink>("game.log"));
    LOG_INFO("Rogalique started");

    // Разрешение/полноэкранный режим — из display_settings.txt (см. DisplaySettings.h); если файла нет, оттуда же
    // берутся значения по умолчанию (1248x768 — прежнее жёстко зашитое разрешение окна). Настройка меняется в
    // экране "Настройки" (см. ниже, settingsSliders) и применяется со следующего запуска — см. класс-комментарий
    // DisplaySettings.h, почему не мгновенно.
    DisplaySettings displaySettings = DisplaySettings::load(displaySettingsPath());
    unsigned windowWidth = static_cast<unsigned>(displaySettings.width);
    unsigned windowHeight = static_cast<unsigned>(displaySettings.height);

    // Сборка сцены И сам игровой цикл (Engine::instance().run() внутри, см. комментарий там) — падение чего-то
    // одного (окно, ресурс, некорректные данные компонента, исключение во время игры) не должно ронять процесс без
    // единого слова в логе.
    try {
        RenderSystem::instance().createWindow(
            windowWidth, windowHeight, "Rogalique", displaySettings.fullscreen, "Resources/Icon/icon.png");
        // Лимит FPS и громкость музыки/эффектов — в отличие от разрешения/полноэкранного режима, применяются сразу
        // (не только "после перезапуска"), но должны применяться и здесь, при СТАРТЕ, а не только когда игрок сам
        // подвигает слайдер в настройках — иначе именно это выглядело как "сбивается после перезапуска" (см.
        // DisplaySettings.h).
        RenderSystem::instance().setFramerateLimit(displaySettings.fpsLimit);
        AudioSystem::instance().setMusicVolume(displaySettings.musicVolume);
        AudioSystem::instance().setEffectsVolume(displaySettings.effectsVolume);
        // В полноэкранном режиме реальный размер окна — разрешение рабочего стола, а не то, что просили (см.
        // RenderSystem::createWindow) — дальнейшая раскладка UI/камеры должна опираться на то, что получилось
        // фактически, иначе на несовпадающем с desktop разрешении всё съедет от границ настоящего окна.
        sf::Vector2u actualWindowSize = RenderSystem::instance().getWindow().getSize();
        windowWidth = actualWindowSize.x;
        windowHeight = actualWindowSize.y;

        // Фоновая музыка движка ничего не знает про конкретный файл путь и цикличность задаёт игровой проект.
        // Стартуем с mainmenu.wav (главное меню) — theme.wav включается отдельно при входе в игру ("Начать"/
        // "Продолжить"/"Загрузить сохранение"/"Играть заново" ниже), mainmenu.wav — при возврате в главное меню
        // (все три "В главное меню"). Раньше theme.wav играл вообще всегда, даже в самом первом главном меню —
        // до этого mainmenu.wav и остальные (gameover.wav/win.wav) лежали в Resources/Sounds неиспользуемыми.
        AudioSystem::instance().playMusic("Resources/Sounds/mainmenu.wav", true);

        // Звуки навигации/подтверждения в меню — те же файлы, что использовал старый Arkanoid (временно).
        AudioSystem::instance().loadSound("ui_move", "Resources/Sounds/rotate.wav");
        AudioSystem::instance().loadSound("ui_confirm", "Resources/Sounds/lineclear.wav");
        // Надеть/снять экипировку в инвентаре (см. InventoryComponent::useBagSlot/unequip) — раньше вообще без
        // звука. swap.wav тоже был unused-остатком от Arkanoid.
        AudioSystem::instance().loadSound("equip", "Resources/Sounds/swap.wav");

        GameObject& root = GameWorld::instance().getRoot();
        // Мировые координаты уровня — фиксированное начало (0,0); окно — "видоискатель" камеры внутри уровня.
        sf::Vector2f levelOrigin(0.f, 0.f);

        // Форма уровня — либо чанк-сборка (хаб Resources/Level/rogalique.tmj + случайные чанки из
        // Resources/Level/Chunks/, см. ChunkAssembler.h), либо, пока в этой папке нет ни одного подходящего
        // чанка (сборщик сам логирует WARN за каждый пропущенный и возвращает false, если в итоге пусто),
        // запасной путь — тот же хаб как единственный цельный уровень, в точности как до появления сборщика чанков.
        // Пересобирается заново при каждом "Начать"/"Играть заново" (см. rebuildLevelGeometry ниже) — раньше
        // форма строилась ровно один раз за весь процесс (был баг — рандом не рандомил локации при перезапуске
        // уровня) — теперь рандом каждый раз и на сам набор комнат/чанков, а не только на то, какой
        // бот/предмет в каком уже готовом слоте (см. rerollContent дальше).
        TiledLevel level;
        ChunkAssemblerConfig chunkConfig;
        // Было: chainLength=2, а комната с ключом занимала ПОСЛЕДНЕЕ звено — то есть заменяла собой один из двух
        // случайных чанков игрока на каждое направление, вдвое срезая разнообразие комнат/EnemySlot/ItemSlot на
        // карте (мало локаций и предметов на карте). Теперь chainLength=3 — оба случайных чанка
        // игрока остаются на месте, комната с ключом добавляется ДОПОЛНИТЕЛЬНЫМ третьим звеном в конце пути.
        chunkConfig.chainLength = 3;
        // По ключу в конце каждой из 4 сторон (см. ChunkAssemblerConfig::keyRoomPaths — гарантированно, не
        // случайным выбором из пула) — открывают главную дверь в хабе, см. Door/DoorComponent ниже.
        chunkConfig.keyRoomPaths = {"Resources/Level/KeyRooms/key_room_north.tmj", "Resources/Level/KeyRooms/key_room_south.tmj",
            "Resources/Level/KeyRooms/key_room_east.tmj", "Resources/Level/KeyRooms/key_room_west.tmj"};
        int levelWidthTiles = 0;
        int levelHeightTiles = 0;

        GameObject& levelContainer = root.addChild(std::make_unique<GameObject>(sf::Vector2f(0.f, 0.f)));

        // Один уже резолвленный тайл слоя (стена/пол/декор — см. TiledLevel.h, почему резолв теперь происходит на
        // этапе загрузки, а не здесь) — рисует ровно тот вырез из тайлсета, что выбрал игрок в Tiled. Якорь —
        // нижний левый угол клетки (x,y), а не центр: для стен/пола (всегда TILE_SIZE x TILE_SIZE) это совпадает с
        // центром клетки, но для декора из "коллекции изображений" (Resources/Map/{barrels,boxes,bones,furniture})
        // с их естественным, часто некратным тайлу размером — это тот же якорь, что использует сам Tiled при
        // отрисовке тайлов крупнее сетки (растут вверх-вправо от клетки, в которую их поставили), то есть то, что
        // игрок видит в редакторе, one-to-one то, что в игре.
        // origin — мировая точка (0,0) карты: levelOrigin для подземелья, arenaOrigin для арены (см.
        // ARENA_LEVEL_PATH ниже) — обе карты рисуются этой же лямбдой, просто с разным смещением.
        auto spawnTiledTileAt
            = [&](sf::Vector2f origin, int x, int y, const ResolvedTile& tile, bool withCollider) {
                  if (!tile.isValid) {
                      return;
                  }
                  float w = static_cast<float>(tile.rect.width);
                  float h = static_cast<float>(tile.rect.height);
                  sf::Vector2f cellBottomLeft(origin.x + x * TILE_SIZE, origin.y + (y + 1) * TILE_SIZE);
                  sf::Vector2f center(cellBottomLeft.x + w / 2.f, cellBottomLeft.y - h / 2.f);
                  auto obj = std::make_unique<GameObject>(center);
                  SpriteComponent& sprite = obj->addComponent<SpriteComponent>(sf::Vector2f(w, h));
                  sprite.setPlaceholderColor(sf::Color(90, 90, 90));
                  sprite.loadTextureRegion(tile.texturePath, tile.rect);
                  // Флаги Flip Horizontally/Vertically/Rotate из Tiled (см. ResolvedTile::flipped* в
                  // TiledLevel.h) — таблица подобрана и проверена побитовым сравнением с эталонным разбором
                  // трёх флагов по спецификации Tiled (сперва диагональный флип-транспонирование, потом
                  // горизонтальный, потом вертикальный): диагональ — поворот на 90° с flipX=V/flipY=!H, без
                  // диагонали — обычные независимые flipX=H/flipY=V без поворота.
                  if (tile.flippedDiagonally) {
                      sprite.setRotation(90.f);
                      sprite.setFlippedX(tile.flippedVertically);
                      sprite.setFlippedY(!tile.flippedHorizontally);
                  } else {
                      sprite.setRotation(0.f);
                      sprite.setFlippedX(tile.flippedHorizontally);
                      sprite.setFlippedY(tile.flippedVertically);
                  }
                  if (withCollider) {
                      obj->addComponent<ColliderComponent>(sf::Vector2f(w, h), true);
                  }
                  levelContainer.addChild(std::move(obj));
              };
        auto spawnTiledTile = [&](int x, int y, const ResolvedTile& tile, bool withCollider) {
            spawnTiledTileAt(levelOrigin, x, y, tile, withCollider);
        };
        auto spawnChest = [&](sf::Vector2f position, const std::string& itemId, int count) {
            const ItemDefinition* item = findItemDefinition(itemId);
            if (!item) {
                LOG_ERROR("SceneFacade: неизвестный id предмета в сундуке \"" + itemId + "\"");
                return;
            }
            levelContainer.addChild(std::make_unique<Chest>(position, *item, count));
        };
        auto torch = [&](sf::Vector2f position) {
            auto prop = std::make_unique<GameObject>(position);
            SpriteComponent& sprite = prop->addComponent<SpriteComponent>(sf::Vector2f(32.f, 32.f));
            sprite.setPlaceholderColor(sf::Color(200, 120, 40));
            sprite.loadAnimation("Resources/Map/Fire/fire_1.png", 3, sf::seconds(0.15f), true);
            levelContainer.addChild(std::move(prop));
        };

        // Слоты/точки, разобранные из слоя объектов (Entities) — заполняются заново при каждом вызове
        // rebuildLevelGeometry ниже (форма/набор комнат меняется, значит и они). Объявлены здесь, а не внутри
        // самой лямбды — их читают buildLevelContent()/rerollContent, определяемые уже ПОСЛЕ первого вызова.
        // FixedEnemy/FixedItem — конкретный бот/предмет в конкретной точке, без рандома (в отличие от
        // EnemySlot/ItemSlot — те просто места, которые заполняются случайно, см. buildLevelContent).
        bool hasPlayerSpawn = false;
        sf::Vector2f playerSpawn(levelOrigin.x + TILE_SIZE, levelOrigin.y + TILE_SIZE);
        std::vector<sf::Vector2f> enemySlotPositions;
        std::vector<sf::Vector2f> itemSlotPositions;
        std::vector<FixedEnemySpec> fixedEnemies;
        std::vector<FixedItemSpec> fixedItems;
        bool hasArrowCrate = false;
        sf::Vector2f arrowCratePosition = playerSpawn;
        GameObject* doorObject = nullptr;

        // Арена волн — отдельная Tiled-карта (Resources/Level/Arena.tmj), грузится и рисуется тем же путём, что и
        // обычное подземелье (см. loadTiledLevel/spawnTiledTileAt ниже), просто со своим набором типов объектов
        // (ArenaSpawnPoint вместо EnemySlot — кольцо появления бойцов волны, PlayerSpawn — точка телепорта игрока
        // с двери, FixedItem/ArrowCrate — аптечки/щит/ящик). Стоит далеко в стороне от обычного подземелья (после
        // ARENA_GAP_TILES тайлов зазора без пола/стен между ними) — дойти туда пешком нельзя, только телепортом
        // при открытии двери (см. DoorComponent::setOnOpened дальше в этой функции). Позиция арены зависит от
        // ширины подземелья (arenaOffsetXTiles), а та меняется при каждой пересборке формы — поэтому вся геометрия
        // арены ниже такое же состояние, пересчитываемое той же лямбдой, что и само подземелье, а не константа.
        constexpr int ARENA_GAP_TILES = 8;
        constexpr const char* ARENA_LEVEL_PATH = "Resources/Level/Arena.tmj";
        TiledLevel arenaLevel;
        int arenaOffsetXTiles = 0;
        sf::Vector2f arenaOrigin(0.f, 0.f);
        // Игрок телепортируется чуть южнее центра, бойцы волны появляются кольцом вокруг центра — не сразу
        // вплотную к точке телепорта. Оба — PlayerSpawn/ArenaSpawnPoint объекты из Arena.tmj, редактируются в
        // Tiled так же, как и всё остальное содержимое арены.
        sf::Vector2f arenaPlayerEntry(0.f, 0.f);
        std::vector<FixedItemSpec> arenaFixedItems;
        bool arenaHasArrowCrate = false;
        sf::Vector2f arenaCratePosition(0.f, 0.f);
        // Свои границы камеры для арены и для подземелья (см. класс-комментарий выше) — переключаются в момент
        // телепорта/сброса (см. DoorComponent::setOnOpened и обработчики меню дальше), а не одна общая на всё
        // сразу, поэтому можно уменьшать/увеличивать арену независимо от того, влезает ли она в самый широкий
        // экран.
        sf::FloatRect dungeonCameraBounds;
        sf::FloatRect arenaCameraBounds;
        std::vector<sf::Vector2f> arenaSpawnPoints;
        int combinedWidthTiles = 0;
        int combinedHeightTiles = 0;

        // Полная пересборка формы уровня — и обычного подземелья (чанки/хаб), и арены волн (её позиция зависит
        // от ширины подземелья): сносит всю геометрию levelContainer (тайлы/Chest/Door/Trap/Pit/факелы, арена) и
        // строит заново с новым сидом, затем NavGrid/ColliderGrid. НЕ трогает актёров/предметы (см. rerollContent
        // дальше) — те создаются/пересоздаются отдельно, уже по СВЕЖИМ спискам слотов, которые эта лямбда здесь
        // заполняет (enemySlotPositions/itemSlotPositions/fixedEnemies/fixedItems/playerSpawn/arrowCratePosition/
        // doorObject). Вызывается один раз при старте сцены (см. ниже) и заново из кнопки "Начать"/"Играть
        // заново" (иначе рандом не рандомил локации при перезапуске уровня).
        auto rebuildLevelGeometry = [&](unsigned shapeSeed) {
            levelContainer.clearChildren();
            doorObject = nullptr;

            if (!assembleChunkedLevel(chunkConfig, shapeSeed, level)) {
                LOG_WARN("SceneFacade: сборка из чанков не удалась (пустая/отсутствующая Resources/Level/Chunks) — "
                         "гружу хаб как цельный уровень");
                if (!loadTiledLevel(chunkConfig.hubPath, level)) {
                    throw std::runtime_error("не удалось загрузить " + chunkConfig.hubPath);
                }
            }
            levelWidthTiles = level.widthTiles;
            levelHeightTiles = level.heightTiles;

            // Порядок отрисовки: пол (низ), стены+коллайдеры, декор, и только потом объекты-сущности (Chest/
            // Door/Trap/Torch/Pit — см. ниже) — у levelContainer нет Y-сортировки (в отличие от actors),
            // рисуется строго в порядке добавления, так что кто добавлен позже, тот и поверх. Декор до сущностей
            // — сущности всегда поверх декора (иначе шипы/ловушки не везде было видно).
            for (int y = 0; y < levelHeightTiles; ++y) {
                for (int x = 0; x < levelWidthTiles; ++x) {
                    if (!level.floorTiles.empty()) {
                        spawnTiledTile(x, y, level.floorTiles[y * levelWidthTiles + x], false);
                    }
                }
            }
            for (int y = 0; y < levelHeightTiles; ++y) {
                for (int x = 0; x < levelWidthTiles; ++x) {
                    if (!level.wallTiles.empty()) {
                        spawnTiledTile(x, y, level.wallTiles[y * levelWidthTiles + x], true);
                    }
                }
            }
            for (int y = 0; y < levelHeightTiles; ++y) {
                for (int x = 0; x < levelWidthTiles; ++x) {
                    if (!level.decorTiles.empty()) {
                        spawnTiledTile(x, y, level.decorTiles[y * levelWidthTiles + x], false);
                    }
                }
            }

            // Разбор слоя объектов (Entities) — по типу объекта (см. TiledLevel.h за полным списком типов и их
            // свойств). PlayerSpawn/EnemySlot/ItemSlot/ArrowCrate/FixedEnemy/FixedItem только собираем в списки —
            // сами объекты (Player/Enemy.../ArrowCrate/предметы) создаются отдельно, в rerollContent.
            hasPlayerSpawn = false;
            playerSpawn = sf::Vector2f(levelOrigin.x + TILE_SIZE, levelOrigin.y + TILE_SIZE);
            enemySlotPositions.clear();
            itemSlotPositions.clear();
            fixedEnemies.clear();
            fixedItems.clear();
            hasArrowCrate = false;
            arrowCratePosition = playerSpawn;
            for (const TiledObject& obj : level.objects) {
                sf::Vector2f position(levelOrigin.x + obj.x, levelOrigin.y + obj.y);
                if (obj.type == "PlayerSpawn") {
                    playerSpawn = position;
                    hasPlayerSpawn = true;
                } else if (obj.type == "EnemySlot") {
                    enemySlotPositions.push_back(position);
                } else if (obj.type == "ItemSlot") {
                    itemSlotPositions.push_back(position);
                } else if (obj.type == "ArrowCrate") {
                    arrowCratePosition = position;
                    hasArrowCrate = true;
                } else if (obj.type == "Chest") {
                    auto idIt = obj.stringProps.find("itemId");
                    auto countIt = obj.intProps.find("count");
                    if (idIt == obj.stringProps.end()) {
                        LOG_ERROR("SceneFacade: объект Chest в Tiled без свойства itemId, пропущен");
                        continue;
                    }
                    spawnChest(position, idIt->second, countIt == obj.intProps.end() ? 1 : countIt->second);
                } else if (obj.type == "Door") {
                    doorObject = &levelContainer.addChild(std::make_unique<Door>(
                        position, std::vector<std::string>{"key_north", "key_south", "key_east", "key_west"}));
                } else if (obj.type == "Trap") {
                    levelContainer.addChild(std::make_unique<Trap>(position));
                } else if (obj.type == "Torch") {
                    torch(position);
                } else if (obj.type == "Pit") {
                    sf::Vector2f pitCenter(position.x + obj.width / 2.f, position.y + obj.height / 2.f);
                    // Необязательное строковое свойство "texture" в Tiled — путь к любой картинке вместо лавы по
                    // умолчанию (см. Pit.h), просто растягивается под размер прямоугольника.
                    auto textureIt = obj.stringProps.find("texture");
                    if (textureIt != obj.stringProps.end() && !textureIt->second.empty()) {
                        levelContainer.addChild(
                            std::make_unique<Pit>(pitCenter, sf::Vector2f(obj.width, obj.height), textureIt->second));
                    } else {
                        levelContainer.addChild(std::make_unique<Pit>(pitCenter, sf::Vector2f(obj.width, obj.height)));
                    }
                } else if (obj.type == "FixedEnemy") {
                    auto kindIt = obj.stringProps.find("kind");
                    if (kindIt == obj.stringProps.end()) {
                        LOG_ERROR("SceneFacade: объект FixedEnemy в Tiled без свойства kind, пропущен");
                        continue;
                    }
                    fixedEnemies.push_back({kindIt->second, position});
                } else if (obj.type == "FixedItem") {
                    auto idIt = obj.stringProps.find("itemId");
                    if (idIt == obj.stringProps.end()) {
                        LOG_ERROR("SceneFacade: объект FixedItem в Tiled без свойства itemId, пропущен");
                        continue;
                    }
                    auto countIt = obj.intProps.find("count");
                    auto interactIt = obj.stringProps.find("requiresInteract");
                    fixedItems.push_back({idIt->second, countIt == obj.intProps.end() ? 1 : countIt->second,
                        interactIt != obj.stringProps.end() && interactIt->second == "true", position});
                } else {
                    LOG_WARN("SceneFacade: объект в Tiled с неизвестным типом \"" + obj.type + "\" пропущен");
                }
            }
            if (!hasPlayerSpawn) {
                LOG_ERROR("SceneFacade: в Tiled-карте нет объекта PlayerSpawn");
                throw std::runtime_error("Resources/Level/rogalique.tmj без PlayerSpawn");
            }
            if (enemySlotPositions.size() + fixedEnemies.size() < 5) {
                LOG_ERROR("SceneFacade: в Tiled-карте меньше 5 объектов EnemySlot+FixedEnemy вместе взятых ("
                          + std::to_string(enemySlotPositions.size()) + "+" + std::to_string(fixedEnemies.size()) + ")");
                throw std::runtime_error("Resources/Level/rogalique.tmj: недостаточно EnemySlot/FixedEnemy");
            }
            if (!hasArrowCrate) {
                LOG_WARN("SceneFacade: в Tiled-карте нет объекта ArrowCrate, ставлю в точку спавна игрока");
            }

            // Загрузка и отрисовка арены — тот же путь, что и подземелье выше (loadTiledLevel + spawnTiledTileAt),
            // просто со своим origin (arenaOrigin, не levelOrigin) и своим набором типов объектов на слое Entities.
            if (!loadTiledLevel(ARENA_LEVEL_PATH, arenaLevel)) {
                throw std::runtime_error(std::string(ARENA_LEVEL_PATH) + " не загрузился");
            }
            arenaOffsetXTiles = levelWidthTiles + ARENA_GAP_TILES;
            arenaOrigin = sf::Vector2f(levelOrigin.x + arenaOffsetXTiles * TILE_SIZE, levelOrigin.y);
            for (int y = 0; y < arenaLevel.heightTiles; ++y) {
                for (int x = 0; x < arenaLevel.widthTiles; ++x) {
                    if (!arenaLevel.floorTiles.empty()) {
                        spawnTiledTileAt(arenaOrigin, x, y, arenaLevel.floorTiles[y * arenaLevel.widthTiles + x], false);
                    }
                }
            }
            for (int y = 0; y < arenaLevel.heightTiles; ++y) {
                for (int x = 0; x < arenaLevel.widthTiles; ++x) {
                    if (!arenaLevel.wallTiles.empty()) {
                        spawnTiledTileAt(arenaOrigin, x, y, arenaLevel.wallTiles[y * arenaLevel.widthTiles + x], true);
                    }
                }
            }
            for (int y = 0; y < arenaLevel.heightTiles; ++y) {
                for (int x = 0; x < arenaLevel.widthTiles; ++x) {
                    if (!arenaLevel.decorTiles.empty()) {
                        spawnTiledTileAt(arenaOrigin, x, y, arenaLevel.decorTiles[y * arenaLevel.widthTiles + x], false);
                    }
                }
            }

            // Разбор объектов арены — свои типы (ArenaSpawnPoint вместо EnemySlot: кольцо появления бойцов волны
            // расставлено вручную в Tiled, без рантайм-отталкивания от предметов/укрытий — сам автор карты видит
            // расстановку и может развести точки на глаз). FixedItem/ArrowCrate читаются тем же способом, что и в
            // подземелье выше, просто в свои arena-переменные (не путать с dungeon-шными fixedItems/hasArrowCrate).
            arenaFixedItems.clear();
            arenaHasArrowCrate = false;
            arenaSpawnPoints.clear();
            bool arenaHasPlayerSpawn = false;
            for (const TiledObject& obj : arenaLevel.objects) {
                sf::Vector2f position(arenaOrigin.x + obj.x, arenaOrigin.y + obj.y);
                if (obj.type == "PlayerSpawn") {
                    arenaPlayerEntry = position;
                    arenaHasPlayerSpawn = true;
                } else if (obj.type == "ArenaSpawnPoint") {
                    arenaSpawnPoints.push_back(position);
                } else if (obj.type == "ArrowCrate") {
                    arenaCratePosition = position;
                    arenaHasArrowCrate = true;
                } else if (obj.type == "FixedItem") {
                    auto idIt = obj.stringProps.find("itemId");
                    if (idIt == obj.stringProps.end()) {
                        LOG_ERROR("SceneFacade: объект FixedItem в Arena.tmj без свойства itemId, пропущен");
                        continue;
                    }
                    auto countIt = obj.intProps.find("count");
                    auto interactIt = obj.stringProps.find("requiresInteract");
                    arenaFixedItems.push_back({idIt->second, countIt == obj.intProps.end() ? 1 : countIt->second,
                        interactIt != obj.stringProps.end() && interactIt->second == "true", position});
                } else {
                    LOG_WARN("SceneFacade: объект в Arena.tmj с неизвестным типом \"" + obj.type + "\" пропущен");
                }
            }
            if (!arenaHasPlayerSpawn) {
                LOG_ERROR("SceneFacade: в Arena.tmj нет объекта PlayerSpawn");
                throw std::runtime_error(std::string(ARENA_LEVEL_PATH) + " без PlayerSpawn");
            }
            if (arenaSpawnPoints.empty()) {
                LOG_ERROR("SceneFacade: в Arena.tmj нет ни одного объекта ArenaSpawnPoint");
                throw std::runtime_error(std::string(ARENA_LEVEL_PATH) + " без ArenaSpawnPoint");
            }

            dungeonCameraBounds
                = sf::FloatRect(levelOrigin.x, levelOrigin.y, levelWidthTiles * TILE_SIZE, levelHeightTiles * TILE_SIZE);
            arenaCameraBounds = sf::FloatRect(
                arenaOrigin.x, arenaOrigin.y, arenaLevel.widthTiles * TILE_SIZE, arenaLevel.heightTiles * TILE_SIZE);
            combinedWidthTiles = arenaOffsetXTiles + arenaLevel.widthTiles;
            combinedHeightTiles = std::max(levelHeightTiles, arenaLevel.heightTiles);

            // NavGrid и пространственная сетка коллайдеров (см. GameWorld::buildColliderGrid/ColliderGrid) —
            // после стен и всех кинематических объектов-препятствий (Pit/арена), уже добавленных выше, те же
            // origin/размер/TILE_SIZE (расширенный, см. combinedWidthTiles/combinedHeightTiles, чтобы охватить и
            // арену тоже). Декор уже на сцене (см. выше, до сущностей) — ему это всё равно безразлично.
            GameWorld::instance().buildNavGrid(levelOrigin, combinedWidthTiles, combinedHeightTiles, TILE_SIZE);
            GameWorld::instance().buildColliderGrid(levelOrigin, combinedWidthTiles, combinedHeightTiles, TILE_SIZE);

            LOG_INFO("Уровень собран: " + std::to_string(levelWidthTiles) + "x" + std::to_string(levelHeightTiles)
                     + " тайлов, объектов: " + std::to_string(level.objects.size()) + ", сид формы " + std::to_string(shapeSeed));
        };

        // Сид ФОРМЫ (какие чанки собрались) — отдельно от сида КОНТЕНТА (какой бот/предмет в каком слоте, см.
        // activeContentSeed дальше). Меняется при каждом "Начать"/"Играть заново" (см. rebuildLevelGeometry
        // выше), а не только один раз за процесс, как раньше — и, в отличие от старого поведения, теперь ещё и
        // сохраняется в GameMemento (см. hasLevelShapeSeed), чтобы "Продолжить"/"Загрузить сохранение" могли
        // восстановить ИМЕННО ТУ форму, при которой сохранялись координаты актёров, а не применить их поверх
        // случайно другой, уже пересобранной.
        std::random_device levelShapeRandomDevice;
        unsigned activeShapeSeed = levelShapeRandomDevice();
        rebuildLevelGeometry(activeShapeSeed);

        // Начальная расстановка ботов/предметов по комнатам — тот же generator, что "Начать" будет звать заново
        // при каждом нажатии (см. ниже) и что "Продолжить"/"Загрузить сохранение" зовут с сидом из сейва.
        std::random_device contentRandomDevice;
        std::mt19937 contentRng(contentRandomDevice());
        unsigned initialContentSeed = contentRng();
        LevelContent levelContent = buildLevelContent(initialContentSeed, enemySlotPositions, fixedEnemies, itemSlotPositions);
        // Сид активного контента — меняется при "Начать"/загрузке; нужен, чтобы "Продолжить"/"Загрузить
        // сохранение" не пересобирали контент заново, если он и так уже совпадает с сохранённым.
        unsigned activeContentSeed = initialContentSeed;
        // Gameplay: явная текущая локация (см. Location.h) — обновляется в performFullReroll/returnToMainMenu
        // (Dungeon), loadFromMemento (по restoredIntoArenaWave) и в DoorComponent::setOnOpened (Arena) ниже.
        Location currentLocation = Location::Dungeon;

        // Подвижные актёры (игрок, враг) — в отдельном контейнере с Y-sort: кто ниже на экране, тот и рисуется поверх, независимо от того, кого добавили в дерево раньше.
        // Ссылку на сам контейнер держим сразу (а не собираем через локальный unique_ptr и не перемещаем его в
        // дерево постфактум, как было раньше) — rerollContent ниже пересоздаёт "лишних" ботов и подбираемые
        // предметы на лету, уже во время игры, ему нужна valid-ссылка на контейнер, а не владеющий указатель.
        GameObject& actorsContainer = root.addChild(std::make_unique<GameObject>(sf::Vector2f(0.f, 0.f)));
        actorsContainer.setSortChildrenByY(true);

        // Точка спавна — объект PlayerSpawn в Tiled (см. разбор объектов выше).
        auto player = std::make_unique<Player>(
            playerSpawn, sf::Vector2f(32.f, 32.f), sf::Vector2f((float)windowWidth, (float)windowHeight));
        HealthComponent* playerHealth = player->getComponent<HealthComponent>();
        InventoryComponent* playerInventory = player->getComponent<InventoryComponent>();
        WeaponComponent* playerWeapon = player->getComponent<WeaponComponent>();
        InputComponent* playerInput = player->getComponent<InputComponent>();
        if (playerInventory) {
            // Без экипировки на старте: 0 брони, спринт/рывок/пистолет закрыты, пока не найдены сапоги/кольцо/арбалет.
            // Перехватчик урона (расходуемая прочность брони, см. InventoryComponent.h) подписывается ниже, вместе
            // с тряской камеры на бронированный удар — там же, где уже есть указатель на camera.
            playerInventory->recomputeEquipmentEffects();
        }
        // Ссылку и точку спавна держим отдельно понадобятся пункту паузы "В главное меню", чтобы вернуть игрока и врага в исходное состояние (HP, позиция, таймеры), а не просто заморозить их как есть.
        GameObject& playerObject = actorsContainer.addChild(std::move(player));

        // Камера игрока (см. Player.cpp) уже следует за ним — теперь ограничиваем её мировыми границами уровня,
        // иначе у края был бы виден "невидимый мир" за пределами построенного пола (см. CameraComponent::setBounds).
        // Указатель держим отдельно (не только на время if) — понадобится ниже (DoorComponent::setOnOpened) и в
        // обработчиках меню, чтобы переключать между dungeonCameraBounds/arenaCameraBounds при телепорте/сбросе.
        CameraComponent* camera = playerObject.getComponent<CameraComponent>();
        if (camera) {
            camera->setBounds(dungeonCameraBounds);
        }
        if (camera && playerHealth) {
            // Graphics: тряска камеры + частицы на изменение HP игрока (см. docs/DESIGN_DOC.md).
            playerObject.addComponent<HealthChangeFeedbackComponent>(*playerHealth, *camera);
        }
        if (camera && playerHealth && playerInventory) {
            // Расходуемая прочность брони (Щит/Шлем/Нагрудник/Штаны, см. InventoryComponent::absorbHit) блокирует
            // удар целиком вместо обычной брони — см. класс-комментарий InventoryComponent.h. HealthComponent сам
            // не знает про InventoryComponent (Engine не зависит от Rogalique), поэтому подписываем сюда.
            // Броня может погасить удар целиком — HP тогда не меняется вовсе, и HealthChangeFeedbackComponent выше
            // (он реагирует только на изменение HP) тряску не даёт, хотя удар физически произошёл. Трясём камеру
            // прямо здесь всякий раз, когда броня забрала хоть часть урона, независимо от того, дошло ли что-то до HP.
            constexpr float ARMOR_HIT_SHAKE_MAGNITUDE_PIXELS = 6.f;
            const sf::Time ARMOR_HIT_SHAKE_DURATION = sf::seconds(0.15f);
            playerHealth->setDamageInterceptor([playerInventory, camera, ARMOR_HIT_SHAKE_DURATION](int damage) {
                int remaining = playerInventory->absorbHit(damage);
                if (remaining < damage) {
                    camera->shake(ARMOR_HIT_SHAKE_MAGNITUDE_PIXELS, ARMOR_HIT_SHAKE_DURATION);
                }
                return remaining;
            });
        }
        // Отрисовщик частиц — прямой ребёнок root (не actorsContainer, чтобы не попасть под её Y-сортировку —
        // у частиц нет единой позиции, определяющей порядок отрисовки), добавлен позже actorsContainer, поэтому
        // рисуется поверх всех существ и предметов.
        root.addChild(std::make_unique<GameObject>(sf::Vector2f(0.f, 0.f)))
            .addComponent<ParticleSystemComponent>();

        // Награда за серию убийств в подземелье: каждые 5 убитых ботов подземелья (не считая
        // детей деления Slime2 — они не самостоятельная угроза, а бонус за уже засчитанного родителя, и не считая
        // вообще ничего на арене волн — см. KillStreakComponent.h) дают +1 к максимуму HP игрока. dungeonKillStreak
        // — счётчик 0..4 к текущей "пятёрке", а не общее число убийств за забег; обнуляется вместе с HealthComponent
        // при полном ребуте (см. performFullReroll/returnToMainMenu ниже) и персистится в сейв (см. GameMemento::
        // hasKillStreakData).
        int dungeonKillStreak = 0;
        auto onDungeonKill = [&dungeonKillStreak, playerHealth] {
            if (!playerHealth) {
                return;
            }
            constexpr int KILLS_PER_BONUS = 5;
            ++dungeonKillStreak;
            if (dungeonKillStreak >= KILLS_PER_BONUS) {
                dungeonKillStreak = 0;
                playerHealth->increaseMaxHp(1);
            }
        };

        // Пять ботов — по одному в каждый из пяти заготовленных слотов-комнат (см. LevelContent), какой именно
        // бот в какой комнате — решает buildLevelContent(seed), см. её же для порядка (0=Enemy,1=Soldier,
        // 2..4=Slime1-3), который здесь используется как есть.
        // Враг рядом с игроком, но вне радиуса обнаружения начнёт преследование, когда игрок подойдёт ближе.
        // Цель врагу отдельно не назначаем: его ChaseComponent сам найдёт игрока по метке ChaseTargetComponent.
        sf::Vector2f enemyPosition = levelContent.enemyPositions[0];
        auto enemy = std::make_unique<Enemy>(enemyPosition, sf::Vector2f(32.f, 32.f), 140.f, 220.f);
        HealthComponent* enemyHealth = enemy->getComponent<HealthComponent>();
        if (enemyHealth) {
            enemy->addComponent<KillStreakComponent>(*enemyHealth, onDungeonKill);
        }
        GameObject& enemyObject = actorsContainer.addChild(std::move(enemy));

        // Второй враг: держит дистанцию и бьёт из лука, а вплотную переходит на меч (см. Soldier.cpp).
        sf::Vector2f soldierPosition = levelContent.enemyPositions[1];
        auto soldier = std::make_unique<Soldier>(soldierPosition, sf::Vector2f(32.f, 32.f), 120.f, 280.f);
        HealthComponent* soldierHealth = soldier->getComponent<HealthComponent>();
        SoldierAmmoComponent* soldierAmmo = soldier->getComponent<SoldierAmmoComponent>();
        if (soldierHealth) {
            soldier->addComponent<KillStreakComponent>(*soldierHealth, onDungeonKill);
        }
        GameObject& soldierObject = actorsContainer.addChild(std::move(soldier));

        // Ящик со стрелами — бесконечная станция пополнения и для Soldier (см. SoldierAmmoComponent), и для
        // игрока (см. WeaponComponent::refillFromNearbyCrate) — объект ArrowCrate в Tiled (см. разбор выше).
        GameObject& crateObject = actorsContainer.addChild(std::make_unique<ArrowCrate>(arrowCratePosition));
        // Второй такой же ящик — на арене волн (см. spawnLevelItems дальше), позиция зависит от arenaOrigin,
        // считается вместе с остальными предметами арены. nullptr до первого вызова spawnLevelItems ниже.
        GameObject* arenaCrateObject = nullptr;

        // Третий враг, три расцветки с разными механиками (см. SlimeConfig в Slime.h): Slime1 — обычная (ближний
        // бой по кругу), Slime2 — при смерти делится на пару мелких копий (childSpawnParent = &actorsContainer, тот
        // же Y-sort контейнер, что и у всех остальных актёров — см. GameWorld::spawnIn), Slime3 — держит
        // дистанцию и плюётся снарядом вместо ближнего боя.
        SlimeConfig slime1Config;
        slime1Config.skin = "Slime1";
        sf::Vector2f slimePosition = levelContent.enemyPositions[2];
        auto slime = std::make_unique<Slime>(slimePosition, sf::Vector2f(28.f, 28.f), 90.f, 220.f, slime1Config);
        HealthComponent* slimeHealth = slime->getComponent<HealthComponent>();
        if (slimeHealth) {
            slime->addComponent<KillStreakComponent>(*slimeHealth, onDungeonKill);
        }
        GameObject& slimeObject = actorsContainer.addChild(std::move(slime));

        SlimeConfig slime2Config;
        slime2Config.skin = "Slime2";
        slime2Config.canSplit = true;
        slime2Config.childSpawnParent = &actorsContainer;
        sf::Vector2f slime2Position = levelContent.enemyPositions[3];
        auto slime2 = std::make_unique<Slime>(slime2Position, sf::Vector2f(28.f, 28.f), 90.f, 220.f, slime2Config);
        HealthComponent* slime2Health = slime2->getComponent<HealthComponent>();
        if (slime2Health) {
            // Считается сам Slime2 (родитель) — его дети от деления (см. SlimeSplitComponent) не получают
            // KillStreakComponent вовсе, они не самостоятельная угроза, а бонус за уже засчитанного родителя.
            slime2->addComponent<KillStreakComponent>(*slime2Health, onDungeonKill);
        }
        GameObject& slime2Object = actorsContainer.addChild(std::move(slime2));

        SlimeConfig slime3Config;
        slime3Config.skin = "Slime3";
        slime3Config.isRanged = true;
        // "Огненная" слизь не бессмертна в перестрелке — выдыхается и умирает сама после 15 плевков, независимо от
        // того, попадали они или нет (см. SlimeShotLimitComponent).
        slime3Config.maxShotsBeforeDeath = 15;
        sf::Vector2f slime3Position = levelContent.enemyPositions[4];
        auto slime3 = std::make_unique<Slime>(slime3Position, sf::Vector2f(28.f, 28.f), 90.f, 220.f, slime3Config);
        HealthComponent* slime3Health = slime3->getComponent<HealthComponent>();
        SlimeShotLimitComponent* slime3ShotLimit = slime3->getComponent<SlimeShotLimitComponent>();
        if (slime3Health) {
            slime3->addComponent<KillStreakComponent>(*slime3Health, onDungeonKill);
        }
        GameObject& slime3Object = actorsContainer.addChild(std::move(slime3));

        // "Лишние" EnemySlot сверх пяти именных ботов (см. LevelContent::extraEnemyPositions) — вместо того чтобы
        // пропадать впустую, туда садятся дополнительные боты. Если известен конкретный вид (дубликат FixedEnemy,
        // см. LevelContent::extraEnemyKinds) — спавнится НАСТОЯЩАЯ копия того вида (второй орк, второй лучник и
        // т.п. — раньше все "лишние" были слаймами, даже дубликат orc/soldier). Анонимное переполнение обычного
        // EnemySlot (kind пуст) — как раньше, крутим по кругу три конфига слаймов. childSpawnParent = &actorsContainer
        // — тот же контейнер, что и у именного Slime2: без этого "лишний" Slime2-конфиг (canSplit=true) выглядел
        // бы неотличимо от именного, но при смерти не делился бы. Дети деления не нужно отслеживать отдельно (как
        // и у именного Slime2) — все они transient и чистятся destroyTransientChildren() наравне с остальными.
        // extraEnemies/extraEnemyPositions — те же переменные, что resetToSpawn/applyExtraEnemiesMemento читают в
        // обработчиках меню дальше; count/kinds этих "лишних" зависит от формы уровня (см. rebuildLevelGeometry),
        // поэтому пересборка (см. rerollContent дальше) не просто переставляет их, а сносит и создаёт заново —
        // отсюда лямбда, а не разовый цикл: он всё равно звался бы дважды (тут и в rerollContent), проще один раз.
        std::vector<sf::Vector2f> extraEnemyPositions;
        std::vector<GameObject*> extraEnemies;
        const SlimeConfig extraSlimeConfigs[] = {slime1Config, slime2Config, slime3Config};
        auto spawnAllExtras = [&] {
            extraEnemyPositions = levelContent.extraEnemyPositions;
            for (std::size_t i = 0; i < extraEnemyPositions.size(); ++i) {
                std::string kind = i < levelContent.extraEnemyKinds.size() ? levelContent.extraEnemyKinds[i] : std::string();
                std::unique_ptr<GameObject> extra
                    = makeEnemyInstance(kind, extraEnemyPositions[i], &actorsContainer, slime1Config, slime2Config, slime3Config);
                if (!extra) {
                    SlimeConfig cfg = extraSlimeConfigs[i % 3];
                    cfg.childSpawnParent = &actorsContainer;
                    extra = std::make_unique<Slime>(extraEnemyPositions[i], sf::Vector2f(28.f, 28.f), 90.f, 220.f, cfg);
                }
                if (auto* extraHealth = extra->getComponent<HealthComponent>()) {
                    extra->addComponent<KillStreakComponent>(*extraHealth, onDungeonKill);
                }
                extraEnemies.push_back(&actorsContainer.addChild(std::move(extra)));
            }
        };
        spawnAllExtras();

        // Предметы на карте — подбираются игроком (см. InventoryComponent/ItemPickupComponent). Как и ArrowCrate,
        // при подборе не уничтожаются, а прячутся; ссылки держим в itemPickups, чтобы вернуть их на место при
        // полном ребуте уровня наравне с crateObject.resetComponents() ниже (сами предметы в мешке при этом не
        // трогаем — инвентарь пока не входит в сохранение, см. GameMemento).
        // requiresInteract=false (мелкие предметы) — подбор проходом рядом; true (ценные находки) — нужен ещё и E,
        // чтобы не подбирались случайно на бегу мимо.
        std::vector<GameObject*> itemPickups;
        auto spawnItemPickup = [&](sf::Vector2f position, const std::string& itemId, int count, bool requiresInteract = false) {
            const ItemDefinition* item = findItemDefinition(itemId);
            if (!item) {
                LOG_ERROR("SceneFacade: неизвестный id предмета \"" + itemId + "\"");
                return;
            }
            itemPickups.push_back(
                &actorsContainer.addChild(std::make_unique<ItemPickup>(position, *item, count, requiresInteract)));
        };
        // Порядок id фиксирован (см. GameMemento::getCollectedPickups() — индекс по порядку спавна), а вот ПОЗИЦИЯ
        // каждого — из levelContent.itemPositions (объекты ItemSlot в Tiled), то есть меняется при каждом "Начать".
        // Первые восемь — все восемь экипируемых категорий разом (гарантия, что каждая есть на карте), остальное —
        // расходники россыпью, дубли разрешены явно. FixedItem — конкретный предмет в конкретной точке (см. разбор
        // объектов в rebuildLevelGeometry), без рандома, вдобавок к ItemSlot-рассыпи. Аптечки/щит/ящик на арене —
        // те же FixedItem/ArrowCrate объекты, только из Arena.tmj (arenaFixedItems/arenaCratePosition, см. ниже) —
        // позиции расставлены в Tiled вручную, без рантайм-проверки расстояния до кольца спавна волны.
        // Одна лямбда на все три группы (как и spawnAllExtras выше) — count/позиции ItemSlot/FixedItem зависят от
        // формы уровня, пересборка сносит и создаёт заново, а не просто переставляет.
        const char* itemIds[] = {"shield", "crossbow", "helmet", "chestplate", "pants", "boots", "ring", "necklace",
            "potion_small", "potion_small", "potion_medium", "potion_medium", "potion_big", "potion_big", "rusty_key",
            "rusty_key", "ancient_skull", "potion_small"};
        constexpr std::size_t itemIdCount = sizeof(itemIds) / sizeof(itemIds[0]);
        auto spawnLevelItems = [&] {
            if (levelContent.itemPositions.size() < itemIdCount) {
                LOG_WARN("SceneFacade: в Tiled-карте меньше ItemSlot (" + std::to_string(levelContent.itemPositions.size())
                         + "), чем предметов для расстановки (" + std::to_string(itemIdCount)
                         + ") — часть предметов не появится");
            }
            for (std::size_t i = 0; i < levelContent.itemPositions.size() && i < itemIdCount; ++i) {
                spawnItemPickup(levelContent.itemPositions[i], itemIds[i], 1);
            }
            for (const FixedItemSpec& fixed : fixedItems) {
                spawnItemPickup(fixed.position, fixed.itemId, fixed.count, fixed.requiresInteract);
            }
            // Аптечки/щит арены — те же FixedItem-объекты из Arena.tmj, что уже разобраны в rebuildLevelGeometry
            // (см. arenaFixedItems выше), той же лямбдой spawnItemPickup, что и обычные FixedItem подземелья.
            for (const FixedItemSpec& fixed : arenaFixedItems) {
                spawnItemPickup(fixed.position, fixed.itemId, fixed.count, fixed.requiresInteract);
            }
            // Тот же принцип, что и главный ArrowCrate в подземелье (см. crateObject выше) — бесконечная станция
            // пополнения болтов для игрока. Указатель держим отдельно (как и crateObject) — при пересборке формы
            // уровня (см. rerollContent дальше) арена сдвигается, и этот ящик просто переставляется на новое
            // место, а не пересоздаётся (в отличие от itemPickups выше — тот не ArrowCrate, а обычный предмет).
            if (arenaHasArrowCrate) {
                if (arenaCrateObject) {
                    arenaCrateObject->setPosition(arenaCratePosition);
                } else {
                    arenaCrateObject = &actorsContainer.addChild(std::make_unique<ArrowCrate>(arenaCratePosition));
                }
            }
        };
        spawnLevelItems();

        // Таблица рекордов прохождения (см. Leaderboard.h) — читаем сразу при старте, чтобы пункт меню "Список
        // лидеров" (см. дальше) мог показать её сразу, без отдельной загрузки по клику.
        Leaderboard leaderboard;
        leaderboard.load(highscoresPath());

        // Секундомер забега (см. GameTimerComponent.h) — в UI-дереве, как и весь остальной HUD, чтобы тикать
        // независимо от паузы мирового дерева (сам он сам разбирается, когда действительно считать, см. update()
        // в .cpp). Обнуляется при каждом входе в игру ("Начать"/"Продолжить"/"Загрузить сохранение"/"Играть
        // заново" ниже), а не только один раз за процесс.
        auto gameTimerObject = std::make_unique<GameObject>(sf::Vector2f(0.f, 0.f));
        GameTimerComponent& gameTimer = gameTimerObject->addComponent<GameTimerComponent>();
        GameWorld::instance().getUIRoot().addChild(std::move(gameTimerObject));

        // true, если игрок уже ввёл имя для ТЕКУЩЕЙ победы (см. nameEntryOverlay ниже и onAllWavesCleared) —
        // экран победы с "Играть заново"/"В главное меню" открывается только после этого, не сразу вместе с
        // isVictory() (раньше вообще не было ввода имени). Сбрасывается в false там же, где и
        // gameTimer, — на каждый новый вход в игру.
        bool victoryNameEntered = false;
        auto nameEntryObject = std::make_unique<GameObject>(sf::Vector2f(0.f, 0.f));
        NameEntryOverlayComponent& nameEntryOverlay
            = nameEntryObject->addComponent<NameEntryOverlayComponent>(sf::Vector2f((float)windowWidth, (float)windowHeight),
                "Resources/GUI/Panel_9Slice_A.png", "Resources/Fonts/Roboto-Bold.ttf", "ПОБЕДА!",
                [&leaderboard, &gameTimer, &victoryNameEntered](const std::string& name) {
                    int seconds = static_cast<int>(gameTimer.getElapsed().asSeconds());
                    leaderboard.addEntry(name, seconds);
                    if (!leaderboard.save(highscoresPath())) {
                        LOG_WARN("NameEntry: не удалось записать таблицу рекордов");
                    }
                    victoryNameEntered = true;
                });
        GameWorld::instance().getUIRoot().addChild(std::move(nameEntryObject));

        // Арена волн (см. arenaLevel/Arena.tmj выше и ArenaWaveComponent.h) — служебный объект без своего
        // визуала, просто дирижирует волнами. spawnEnemy — makeEnemyInstance() (тот же, что и у "лишних" ботов
        // выше), но добавляет через GameWorld::spawnIn(), не actorsContainer.addChild() напрямую — спавн во время
        // уже идущей игры (а не на старте сцены) обязан идти через отложенную очередь, см. класс-комментарий
        // GameWorld::spawnIn().
        auto arenaWaveObject = std::make_unique<GameObject>(sf::Vector2f(0.f, 0.f));
        ArenaWaveComponent& arenaWaves = arenaWaveObject->addComponent<ArenaWaveComponent>(
            std::vector<std::vector<ArenaWaveComponent::WaveEnemySpec>>{
                {{"slime1", 4}},
                {{"slime2", 4}, {"slime3", 4}},
                {{"soldier", 4}, {"orc", 6}},
                {{"boss", 1}},
            },
            // Разная музыка на каждую волну — свой трек под каждую из четырёх, включая отдельный
            // boss.wav на волну босса.
            std::vector<std::string>{"Resources/Sounds/wave1.wav", "Resources/Sounds/wave2.wav", "Resources/Sounds/wave3.wav",
                "Resources/Sounds/boss.wav"},
            arenaSpawnPoints,
            [&actorsContainer, slime1Config, slime2Config, slime3Config, &arenaCameraBounds](
                const std::string& kind, sf::Vector2f position) -> GameObject* {
                // TransientComponent — тот же приём, что у детей деления слизи: полный ребут уровня ("В главное
                // меню"/"Начать заново"/загрузка, см. destroyTransientChildren() ниже по коду) должен убирать
                // бойцов волны вместе со всем остальным динамически заспавненным, а не оставлять их висеть в
                // actorsContainer навсегда.
                if (kind == "boss") {
                    // Подкрепление босса — VampireSpawnMinion (та же тема пака, что и у самого босса, см. Boss.cpp),
                    // заспавненные тем же приёмом (TransientComponent + GameWorld::spawnIn), что и сам босс и
                    // остальные бойцы волны — BossMinionSummonComponent просто зовёт эту фабрику по своему таймеру.
                    auto spawnMinion = [&actorsContainer](sf::Vector2f minionPosition) -> GameObject* {
                        static std::mt19937 minionRng{std::random_device{}()};
                        bool feminine = minionRng() % 2 == 0;
                        auto minion = std::make_unique<VampireSpawnMinion>(
                            minionPosition, sf::Vector2f(28.f, 28.f), 90.f, 220.f, feminine);
                        minion->addComponent<TransientComponent>();
                        GameObject* rawMinion = minion.get();
                        GameWorld::instance().spawnIn(actorsContainer, std::move(minion));
                        return rawMinion;
                    };
                    // Коллайдер (не VISUAL_SIZE, тот отдельно и больше — см. Boss.cpp) — АТАКА игрока/босса меряет
                    // дистанцию между ЦЕНТРАМИ GameObject (см. AttackComponent::findTarget), коллайдер сюда не
                    // входит вовсе, но ФИЗИЧЕСКИ подойти ближе, чем сумма половин коллайдеров игрока (32x32) и
                    // босса, тоже нельзя — с прежними 96x96 это давало ~64px минимальной дистанции против
                    // PLAYER_ATTACK_RANGE=56 (Player.cpp), и удар мечом физически не мог долететь (был баг —
                    // удары не проходили). 84x84 — крупнее прежних 64x64 (коллайдер визуально терялся на фоне
                    // VISUAL_SIZE=192x192) — вместе с
                    // увеличенным PLAYER_ATTACK_RANGE=70 (см. Player.cpp) по-прежнему с запасом умещается в
                    // дистанцию удара (~58px минимум против 70px дальности).
                    // Спавнится не на случайной точке из arenaSpawnPoints (см. position-параметр), а строго в
                    // центре арены — эффектнее, чем появление где-то на случайной точке.
                    sf::Vector2f arenaCenter(arenaCameraBounds.left + arenaCameraBounds.width / 2.f,
                        arenaCameraBounds.top + arenaCameraBounds.height / 2.f);
                    // Скорость 130, не 70 (буст сложности) — раньше был
                    // медленнее любого рядового врага (Enemy=140/Soldier=120/Slime=90, см. вызовы выше), теперь
                    // сопоставим с Soldier, ощутимо быстрее, но всё ещё чуть медленнее орка/игрока.
                    auto boss = std::make_unique<Boss>(arenaCenter, sf::Vector2f(84.f, 84.f), 130.f, 260.f, spawnMinion);
                    // Эффект появления — тот же приём и цвет, что и у телепорта (см. BossTeleportComponent), чтобы
                    // визуально читалось как одна и та же "магия босса", а не два разных случайных эффекта.
                    ParticleSystem::instance().spawnBurst(
                        arenaCenter, 30, sf::Color(170, 60, 210), 60.f, 180.f, 3.f, 7.f, sf::seconds(0.6f));
                    // "Фаза ярости" на низком HP — телепорт в случайную точку арены, с отступом в 2 тайла от стен,
                    // чтобы не телепортировало прямо в стену.
                    // Порог 30% (было 20%) и интервал 6с (было 10с) — буст сложности, фаза ярости включается
                    // раньше и телепортирует чаще.
                    if (auto* bossHealth = boss->getComponent<HealthComponent>()) {
                        constexpr float ARENA_WALL_MARGIN = 2.f * TILE_SIZE;
                        sf::FloatRect teleportBounds(arenaCameraBounds.left + ARENA_WALL_MARGIN,
                            arenaCameraBounds.top + ARENA_WALL_MARGIN, arenaCameraBounds.width - 2.f * ARENA_WALL_MARGIN,
                            arenaCameraBounds.height - 2.f * ARENA_WALL_MARGIN);
                        boss->addComponent<BossTeleportComponent>(*bossHealth, 0.3f, sf::seconds(6.f), teleportBounds);
                    }
                    boss->addComponent<TransientComponent>();
                    GameObject* rawBoss = boss.get();
                    GameWorld::instance().spawnIn(actorsContainer, std::move(boss));
                    attachSpawnFade(rawBoss);
                    return rawBoss;
                }
                std::unique_ptr<GameObject> enemy
                    = makeEnemyInstance(kind, position, &actorsContainer, slime1Config, slime2Config, slime3Config);
                if (!enemy) {
                    LOG_ERROR("ArenaWaveComponent: неизвестный kind=\"" + kind + "\" в составе волны, пропущен");
                    return nullptr;
                }
                enemy->addComponent<TransientComponent>();
                GameObject* raw = enemy.get();
                GameWorld::instance().spawnIn(actorsContainer, std::move(enemy));
                attachSpawnFade(raw);
                return raw;
            },
            [&nameEntryOverlay, &gameTimer] {
                // Игрок мог погибнуть уже ПОСЛЕ того, как выбита последняя волна, но ДО того, как отыграл
                // victoryDelay (снаряд/оставшийся моб доехал до него в эти секунды, см. класс-комментарий
                // victoryDelay в ArenaWaveComponent.h) — экран поражения к этому моменту уже показан
                // (PlayerDeathComponent::update сам гасит обратный случай, см. её комментарий), поверх него победу
                // не включаем (был баг — оба экрана могли выставиться разом, поражение сменялось победой).
                if (GameWorld::instance().isGameOver()) {
                    return;
                }
                // false — не зацикливать: win.wav не короткий джингл, а полноценный трек, доиграет один раз и
                // затихнет сам, пока игрок читает экран победы (win.wav раньше лежал неиспользуемым).
                AudioSystem::instance().playMusic("Resources/Sounds/win.wav", false);
                GameWorld::instance().setVictory(true);
                // Мир ставим на паузу только теперь — анимация смерти босса уже доиграла ДО этого вызова (см.
                // BOSS_VICTORY_DELAY ниже и класс-комментарий victoryDelay в ArenaWaveComponent.h), так что здесь,
                // в отличие от isGameOver() (тот нарочно не ставит мир на паузу сразу — см. PlayerDeathComponent.h),
                // ничего доигрывать уже не нужно.
                GameWorld::instance().setPaused(true);
                // Экран победы с "Играть заново"/"В главное меню" (см. victoryItems дальше) открывается только
                // ПОСЛЕ того, как имя введено (см. victoryNameEntered/onConfirm выше) — сразу показываем именно
                // экран ввода имени, а не оба разом.
                nameEntryOverlay.show({
                    "Вы прошли игру! Время: " + formatElapsedTime(gameTimer.getElapsed()),
                    "Введите имя для таблицы лидеров:",
                });
                LOG_INFO("ArenaWaveComponent: все волны выбиты — победа");
            },
            // Пауза перед экраном победы — успевает доиграть анимация смерти босса (Boss.cpp: DEATH_FRAME_COUNT=57
            // кадров по 0.04с ≈ 2.28с), а не обрывается на первом кадре внезапно появившимся экраном победы.
            // Небольшой запас (2.4с) на случай, если кадр начал играть не с самого начала update().
            BOSS_VICTORY_DELAY,
            // Разброс появления бойцов ОДНОЙ волны во времени (см. класс-комментарий spawnStagger в
            // ArenaWaveComponent.h) — вместе с attachSpawnFade() выше даёт эффект "волна затекает", а не
            // мгновенно материализуется целиком.
            ARENA_SPAWN_STAGGER);
        root.addChild(std::move(arenaWaveObject));

        // Дверь открывается (см. DoorComponent::update()) — вместо мгновенной победы (как было раньше, 2 ключа)
        // телепортирует игрока на арену и запускает волну 1; победа теперь только после того, как выбита
        // последняя (см. onAllWavesCleared выше). Своя лямбда (не разовый if/else) — doorObject пересоздаётся
        // заново каждой пересборкой формы уровня (см. rebuildLevelGeometry — он живёт в levelContainer, которую
        // та лямбда полностью сносит), а значит и колбэк нужно навешивать на НОВЫЙ объект каждый раз заново, а не
        // один раз при старте сцены. Читает arenaPlayerEntry/arenaCameraBounds/camera на момент СВОЕГО вызова —
        // если позвать снова (после пересборки, когда эти значения уже другие), захватит свежие.
        auto wireDoorCallback = [&] {
            if (!doorObject) {
                LOG_WARN("SceneFacade: в Tiled-карте нет объекта Door — арена волн ничем не запустится");
                return;
            }
            auto* doorComponent = doorObject->getComponent<DoorComponent>();
            if (!doorComponent) {
                return;
            }
            doorComponent->setOnOpened(
                [&playerObject, &arenaWaves, arenaPlayerEntry, camera, arenaCameraBounds, &currentLocation] {
                    // Свои границы у арены (см. arenaCameraBounds выше) — не общие с подземельем, так что размер
                    // арены не обязан быть больше самого широкого экрана (был баг с камерой на этом месте).
                    if (camera) {
                        camera->setBounds(arenaCameraBounds);
                    }
                    playerObject.setPosition(arenaPlayerEntry);
                    arenaWaves.start();
                    currentLocation = Location::Arena;
                });
        };
        wireDoorCallback();

        // Перемешивает содержимое уровня заново под новый сид: кто из пяти ботов в какой из пяти комнат-слотов
        // (и заодно обновляет *Position-переменные — их читают applyOptionalMemento/resetToSpawn при выходе в
        // главное меню/смерти, чтобы "точка спавна" совпадала с последней рассадкой, а не с самой первой), и в
        // каком слоте какой предмет на карте. Раньше просто переставляла уже существующие extraEnemies/itemPickups
        // по новым позициям (их количество не менялось — форма уровня была фиксирована за весь процесс). Теперь,
        // когда форма тоже пересобирается (см. rebuildLevelGeometry/rerollContent в кнопке "Начать" ниже), число и
        // виды "лишних" ботов/предметов сами могут измениться — поэтому extraEnemies/itemPickups сносятся целиком
        // (GameObject::destroyChild) и создаются заново (spawnAllExtras/spawnLevelItems), а не просто двигаются.
        auto rerollContent = [&](unsigned seed) {
            levelContent = buildLevelContent(seed, enemySlotPositions, fixedEnemies, itemSlotPositions);
            enemyPosition = levelContent.enemyPositions[0];
            enemyObject.setPosition(enemyPosition);
            soldierPosition = levelContent.enemyPositions[1];
            soldierObject.setPosition(soldierPosition);
            slimePosition = levelContent.enemyPositions[2];
            slimeObject.setPosition(slimePosition);
            slime2Position = levelContent.enemyPositions[3];
            slime2Object.setPosition(slime2Position);
            slime3Position = levelContent.enemyPositions[4];
            slime3Object.setPosition(slime3Position);

            for (GameObject* extra : extraEnemies) {
                actorsContainer.destroyChild(extra);
            }
            extraEnemies.clear();
            spawnAllExtras();

            for (GameObject* pickup : itemPickups) {
                actorsContainer.destroyChild(pickup);
            }
            itemPickups.clear();
            spawnLevelItems();

            crateObject.setPosition(arrowCratePosition);

            activeContentSeed = seed;
            LOG_INFO("Контент уровня перемешан заново, сид " + std::to_string(seed));
        };

        // Полный рандомный перезапуск — и формы уровня, и содержимого, каждый под свой свежий std::random_device
        // (иначе рандом не рандомил локации при перезапуске уровня). Общий хвост для кнопок "Начать" и
        // "Играть заново" — раньше он был продублирован в обеих почти дословно.
        auto performFullReroll = [&] {
            std::random_device shapeDevice;
            activeShapeSeed = shapeDevice();
            rebuildLevelGeometry(activeShapeSeed);
            wireDoorCallback();
            std::random_device contentDevice;
            rerollContent(contentDevice());
            // Свежий уровень — всегда подземелье, арена ещё не открыта (см. Location.h).
            currentLocation = Location::Dungeon;
            // Новый забег — награда за прошлую серию убийств (см. KillStreakComponent/onDungeonKill выше) хранится
            // не в компоненте, а в этой локальной переменной сцены, resetComponents() ниже её не тронет — сбрасываем
            // явно, отдельной строкой.
            dungeonKillStreak = 0;
            // Полный сброс компонентов игрока и пятёрки именных ботов подземелья — тот же resetComponents(), что
            // и у "В главное меню" (см. resetToSpawn/returnToMainMenu), просто без setPosition (её сразу после
            // этого зовёт вызывающий код — playerSpawn у игрока, новые слоты у ботов уже расставил rerollContent
            // выше). Раньше здесь трогали только HP игрока (playerHealth->reset()) — экипировка/мешок игрока и
            // компоненты пяти именных ботов не сбрасывались вовсе, поэтому "Играть заново" после победы оставляло
            // игрока со старой снарягой, а уже убитых в подземелье ботов — мёртвыми (баг, "Играть заново" на деле
            // продолжало прошлый забег, а не начинало чистый).
            playerObject.resetComponents();
            enemyObject.resetComponents();
            soldierObject.resetComponents();
            slimeObject.resetComponents();
            slime2Object.resetComponents();
            slime3Object.resetComponents();
            crateObject.resetComponents();
        };

        // Если сейв был сделан при другой форме уровня — пересобирает её под тот же сид, что и при сохранении, и
        // возвращает true. Общий хвост для "Продолжить"/"Загрузить сохранение" — раньше был продублирован в обеих.
        auto rerollShapeIfNeeded = [&](const GameMemento& memento) {
            if (!memento.hasLevelShapeSeed() || memento.getLevelShapeSeed() == activeShapeSeed) {
                return false;
            }
            activeShapeSeed = memento.getLevelShapeSeed();
            rebuildLevelGeometry(activeShapeSeed);
            wireDoorCallback();
            return true;
        };

        // Общий resync всех клавиш-эджей, которые не polled, пока мир на паузе (InputComponent/WeaponComponent/
        // PlayerAttackComponent висят на игроке, ItemPickupComponent — на предметах на карте, ChestComponent — на
        // сундуках) — без этого клавиша, зажатая всё время паузы и отпущенная уже после снятия, на первом кадре
        // геймплея прочиталась бы как "только что нажали" (тот же класс бага, что чинили у прыжка после
        // подтверждения меню в начале сессии). Один общий хвост вместо копии в каждой из точек снятия паузы ниже.
        auto resyncPlayerInput = [&playerObject] {
            if (auto* input = playerObject.getComponent<InputComponent>()) {
                input->resyncInput();
            }
            if (auto* weapon = playerObject.getComponent<WeaponComponent>()) {
                weapon->resyncInput();
            }
            if (auto* attack = playerObject.getComponent<PlayerAttackComponent>()) {
                attack->resyncInput();
            }
            for (ItemPickupComponent* pickup : GameWorld::instance().getRoot().getComponentsInChildren<ItemPickupComponent>()) {
                pickup->resyncInteract();
            }
            resyncInteractables();
        };

        // Экран титров.
        auto creditsObject = std::make_unique<GameObject>(sf::Vector2f(0.f, 0.f));
        std::vector<std::string> creditsLines = {
            "Автор, геймдизайнер и программист",
            "@player6653",
            "",
            "Графика (локации и персонажи)",
            "@Sscary_Gameart",
            "",
            "GUI",
            "@tiopalada",
            "",
            "Иконки предметов",
            "@shikashipx",
            "",
            "Враги",
            "@Zerie",
            "",
            "Босс",
            "@ElectricLemon",
            "",
            "Музыка",
            "@Bocuma",
        };
        CreditsOverlayComponent& credits = creditsObject->addComponent<CreditsOverlayComponent>(
            sf::Vector2f((float)windowWidth, (float)windowHeight), "Resources/GUI/Panel_9Slice_A.png",
            "Resources/Fonts/Roboto-Regular.ttf", std::vector<CreditsOverlayComponent::Page>{{"ТИТРЫ", creditsLines}});

        // Экран Помощь — четыре страницы: управление, противники, предметы, цель игры; листаются стрелками или
        // Left/Right. Обновлено под текущее состояние игры (4 ключа/дверь/арена волн, актуальные урон/HP/прочность
        // — старый текст оставался от версии до брони-через-экипировку и до арены). CreditsOverlayComponent
        // сам переносит длинные строки по словам под ширину панели (см. Engine/CreditsOverlayComponent.cpp) — раньше
        // не переносил вовсе, отсюда и "текст вылезает за края" на странице предметов (самые длинные строки).
        auto helpObject = std::make_unique<GameObject>(sf::Vector2f(0.f, 0.f));
        std::vector<std::string> controlsPage = {
            "WASD / стрелки — движение",
            "Shift — спринт (нужны Сапоги)",
            "Ctrl / X — рывок (нужно Кольцо)",
            "Space / C — прыжок",
            "F / Z / ЛКМ — атака копьём / выстрел из арбалета",
            "Q / колесо мыши — сменить оружие",
            "R — перезарядка арбалета",
            "Tab / ПКМ — инвентарь (ПКМ и закрывает)",
            "E — взаимодействие (сундук, дверь, находка)",
            "Escape — пауза",
        };
        // Цифры сверены с константами в Player.cpp/Enemy.cpp/Soldier.cpp/Slime.cpp — броня у игрока теперь только
        // от надетой экипировки (см. страницу "ПРЕДМЕТЫ"), плоской брони/снижения урона больше нет ни у кого.
        std::vector<std::string> statsPage = {
            "Игрок — 4 HP, броня только от надетой экипировки",
            "Копьё — урон 2, кулдаун 0.5с",
            "Арбалет — урон 2, 1 болт + 10 запасных, перезарядка 1с",
            "Смертельный удар при HP > 1 оставляет 1 HP — добить можно только вторым",
            "Каждые 5 убитых ботов подземелья — +1 к максимуму HP",
            "",
            "Орк — 6 HP, ближний бой, урон удара 2",
            "Лучник — 4 HP, держит дистанцию, лук и меч по 1 урону",
            "Слизь (3 расцветки) — 2 HP, урон 1",
            "Синяя слизь делится надвое при смерти",
            "Огненная слизь плюётся и выдыхается после 15 плевков",
            "",
            "Босс (арена, волна 4) — 50 HP, бьёт мечом (3) и магией (2) одновременно",
            "Изредка кусает (лечит себя на весь урон) или бьёт по площади с заметным замахом",
            "Изредка стреляет усиленным зарядом или залпом из двух колец снарядов сразу во все стороны",
            "Призывает подкрепление (Vampire Spawn, 3 HP) каждые 9 секунд боя",
            "На HP ≤ 30% раз в 6 секунд телепортируется по арене",
        };
        // Компактно, по предмету на строку — что лежит на этом уровне и что делает (см. ItemDefinition.cpp).
        std::vector<std::string> itemsPage = {
            "Зелья лечения — маленькое +1, обычное +2, большое +4 HP",
            "4 ключа (стороны света) — открывают главную дверь в хабе",
            "Ржавый ключ, Древний череп (E) — пока просто лежат",
            "Щит(5)/Нагрудник(3)/Штаны(2)/Шлем(1) — блокируют удары по очереди, потом ломаются",
            "Сапоги — открывают спринт (Shift). Кольцо — открывает рывок (Ctrl)",
            "Ожерелье — пассивная регенерация: +1 HP раз в 4 секунды, пока надето",
            "Арбалет — второе оружие (Q), 1 болт, перезарядка R",
            "",
            "Клик по предмету в мешке — использовать / надеть",
            "Клик по занятой ячейке экипировки — снять",
        };
        // Новая страница — раньше про арену/волны/победу в Помощи не было ни слова.
        std::vector<std::string> goalPage = {
            "Соберите все 4 ключа — Северный/Южный/Восточный/Западный",
            "Отнесите их к главной двери в центре хаба",
            "Дверь откроется и телепортирует на арену на выживание",
            "",
            "Волна 1 — 4 обычные слизи",
            "Волна 2 — 4 синие + 4 огненные слизи",
            "Волна 3 — 4 лучника + 6 орков",
            "Волна 4 — босс с призывом подкрепления",
            "Следующая волна начинается после зачистки предыдущей",
            "Победа — после разгрома последней волны",
        };
        CreditsOverlayComponent& help
            = helpObject->addComponent<CreditsOverlayComponent>(sf::Vector2f((float)windowWidth, (float)windowHeight),
                "Resources/GUI/Panel_9Slice_A.png", "Resources/Fonts/Roboto-Regular.ttf",
                std::vector<CreditsOverlayComponent::Page>{
                    {"УПРАВЛЕНИЕ", controlsPage}, {"ПРОТИВНИКИ", statsPage}, {"ПРЕДМЕТЫ", itemsPage}, {"ЦЕЛЬ ИГРЫ", goalPage}},
                "Resources/GUI/Button_ArrowLeft_Sheet.png", "Resources/GUI/Button_ArrowRight_Sheet.png", "ui_move");

        // Экран "Список лидеров" (по образцу старого Arkanoid-проекта этого автора, см. Leaderboard.h)
        // — переиспользует CreditsOverlayComponent (как титры/помощь), но с ОДНОЙ страницей, содержимое которой
        // меняется между показами (новый рекорд мог появиться после победы) — отсюда setPages() при каждом show()
        // через showLeaderboard ниже, а не один раз в конструкторе, как у титров/помощи.
        auto leaderboardObject = std::make_unique<GameObject>(sf::Vector2f(0.f, 0.f));
        CreditsOverlayComponent& leaderboardOverlay = leaderboardObject->addComponent<CreditsOverlayComponent>(
            sf::Vector2f((float)windowWidth, (float)windowHeight), "Resources/GUI/Panel_9Slice_A.png",
            "Resources/Fonts/Roboto-Regular.ttf", std::vector<CreditsOverlayComponent::Page>{});
        auto showLeaderboard = [&leaderboard, &leaderboardOverlay] {
            const std::vector<Leaderboard::Entry>& entries = leaderboard.getEntries();
            std::vector<std::string> lines;
            if (entries.empty()) {
                lines.push_back("(пока пусто — пройдите игру первым)");
            } else {
                constexpr std::size_t MAX_SHOWN = 10;
                for (std::size_t i = 0; i < entries.size() && i < MAX_SHOWN; ++i) {
                    lines.push_back(std::to_string(i + 1) + ". " + entries[i].name + " — "
                                    + formatElapsedTime(sf::seconds(static_cast<float>(entries[i].timeSeconds))));
                }
            }
            leaderboardOverlay.setPages({{"СПИСОК ЛИДЕРОВ", lines}});
            leaderboardOverlay.show();
        };

        // Экран Настройки.
        auto settingsObject = std::make_unique<GameObject>(sf::Vector2f(0.f, 0.f));
        std::vector<SettingsOverlayComponent::Slider> settingsSliders;
        // Слайдеры звука/FPS применяются мгновенно (см. run() выше — так же применяются при старте) И, в отличие
        // от прежней версии, сразу пишутся на диск через displaySettings.save() — раньше меняли только живые
        // RenderSystem/AudioSystem, ничего не сохраняя, и следующий запуск снова видел значения по умолчанию (баг
        // "лимит FPS и звук сбиваются после перезапуска").
        settingsSliders.push_back({"Музыка", [] { return AudioSystem::instance().getMusicVolume(); },
            [&displaySettings](float v) {
                AudioSystem::instance().setMusicVolume(v);
                displaySettings.musicVolume = v;
                displaySettings.save(displaySettingsPath());
            }});
        settingsSliders.push_back({"Эффекты", [] { return AudioSystem::instance().getEffectsVolume(); },
            [&displaySettings](float v) {
                AudioSystem::instance().setEffectsVolume(v);
                displaySettings.effectsVolume = v;
                displaySettings.save(displaySettingsPath());
            }});
        settingsSliders.push_back(
            {"Лимит FPS", [] { return currentFpsLimitIndex() / static_cast<float>(FPS_LIMIT_OPTION_COUNT - 1); },
                [&displaySettings](float v) {
                    int index = static_cast<int>(std::lround(v * (FPS_LIMIT_OPTION_COUNT - 1)));
                    index = std::max(0, std::min(FPS_LIMIT_OPTION_COUNT - 1, index));
                    RenderSystem::instance().setFramerateLimit(FPS_LIMIT_OPTIONS[index]);
                    displaySettings.fpsLimit = FPS_LIMIT_OPTIONS[index];
                    displaySettings.save(displaySettingsPath());
                },
                1.f / static_cast<float>(FPS_LIMIT_OPTION_COUNT - 1),
                [] {
                    unsigned limit = RenderSystem::instance().getFramerateLimit();
                    return limit == 0 ? std::string("без ограничения") : std::to_string(limit) + " FPS";
                }});
        // Разрешение/полноэкранный режим — см. класс-комментарий DisplaySettings.h, почему не мгновенно (сохраняем
        // на диск, реально применяется при следующем запуске игры, отсюда пометка "после перезапуска" в тексте).
        settingsSliders.push_back({"Разрешение",
            [&displaySettings] {
                return currentResolutionIndex(displaySettings) / static_cast<float>(RESOLUTION_OPTION_COUNT - 1);
            },
            [&displaySettings](float v) {
                int index = static_cast<int>(std::lround(v * (RESOLUTION_OPTION_COUNT - 1)));
                index = std::max(0, std::min(RESOLUTION_OPTION_COUNT - 1, index));
                displaySettings.width = RESOLUTION_OPTIONS[index].width;
                displaySettings.height = RESOLUTION_OPTIONS[index].height;
                displaySettings.save(displaySettingsPath());
            },
            1.f / static_cast<float>(RESOLUTION_OPTION_COUNT - 1),
            [&displaySettings] {
                const ResolutionOption& r = RESOLUTION_OPTIONS[currentResolutionIndex(displaySettings)];
                return std::to_string(r.width) + "x" + std::to_string(r.height) + " (после перезапуска)";
            }});
        settingsSliders.push_back({"Полный экран", [&displaySettings] { return displaySettings.fullscreen ? 1.f : 0.f; },
            [&displaySettings](float v) {
                displaySettings.fullscreen = v >= 0.5f;
                displaySettings.save(displaySettingsPath());
            },
            1.f, [&displaySettings] { return std::string(displaySettings.fullscreen ? "да" : "нет") + " (после перезапуска)"; }});
        SettingsOverlayComponent& settings = settingsObject->addComponent<SettingsOverlayComponent>(
            sf::Vector2f((float)windowWidth, (float)windowHeight), "Resources/GUI/Panel_9Slice_A.png",
            "Resources/Fonts/Roboto-Regular.ttf", "НАСТРОЙКИ", std::move(settingsSliders));

        // Фон главного меню.
        auto mainMenuBackgroundObject = std::make_unique<GameObject>(sf::Vector2f(0.f, 0.f));
        mainMenuBackgroundObject->addComponent<TiledBackgroundComponent>(sf::Vector2f((float)windowWidth, (float)windowHeight),
            "Resources/Map/RpgMaker_48x48/img/tilesets/Dungeon_Floors.png", sf::IntRect(480, 0, 48, 48),
            [] { return !GameWorld::instance().hasStarted(); });
        GameWorld::instance().getUIRoot().addChild(std::move(mainMenuBackgroundObject));
        LOG_INFO("UI: фон главного меню добавлен");

        auto mainMenuObject = std::make_unique<GameObject>(sf::Vector2f(0.f, 0.f));
        std::vector<MenuOverlayComponent::Item> mainMenuItems;

        // Общая логика "Продолжить"/"Загрузить сохранение" (см. оба использования ниже) — раньше были почти
        // дословной копией друг друга (~85 строк каждая, отличались буквально в двух местах). resetActorsFirst
        // отражает единственное содержательное различие: "Загрузить сохранение" приходит с экрана поражения, где
        // актёры могли остаться в "мёртвом"/визуально рассинхронизированном состоянии — им нужен полный
        // resetComponents() ДО применения позиций/HP из сейва; "Продолжить" зовётся из уже активной игры, этот
        // сброс ни к чему (тот же actorHealth/аниматор и так корректны, просто позиция/HP перезаписываются). Ящик
        // со стрелами/сундуки/предметы на карте в сохранение не входят сами по себе — сбрасываются безусловно в
        // обоих случаях, независимо от resetActorsFirst (порядок относительно player/enemy/soldier/slime* тут не
        // важен — это независимые объекты).
        auto loadFromMemento
            = [&playerObject, &enemyObject, &soldierObject, &crateObject, &slimeObject, &slime2Object, &slime3Object,
                  &actorsContainer, &itemPickups, &resyncPlayerInput, &activeContentSeed, &rerollContent, playerHealth,
                  enemyHealth, soldierHealth, slimeHealth, slime2Health, slime3Health, &soldierPosition, &slimePosition,
                  &slime2Position, &slime3Position, playerInventory, soldierAmmo, slime3ShotLimit, &extraEnemies,
                  &extraEnemyPositions, &arenaWaves, camera, &dungeonCameraBounds, &arenaCameraBounds, &rerollShapeIfNeeded,
                  &gameTimer, &victoryNameEntered, &currentLocation, &dungeonKillStreak](
                  const std::string& actionLabel, bool resetActorsFirst) {
                  GameMemento memento;
                  if (!GameMemento::load(savePath(), memento)) {
                      LOG_WARN(actionLabel + ": сохранение не найдено или повреждено");
                      return;
                  }
                  // Сейв мог быть сделан при другой форме уровня (см. hasLevelShapeSeed — какие чанки/комнаты вообще
                  // собрались) — тогда пересобираем форму заново под тот же сид, что был при сохранении, ДО перемешивания
                  // контента и применения позиций из сейва: иначе слоты/спавн-точки уже не совпадали бы с тем, что сейв
                  // на самом деле помнит (игрок оказался бы внутри стены другой, случайно текущей формы). Без данных о
                  // сиде формы (сейв старее этого поля) оставляем ту форму, что уже собрана в этом запуске процесса.
                  bool shapeChanged = rerollShapeIfNeeded(memento);
                  // Сейв мог быть сделан и при другой случайной рассадке ботов/предметов ПО УЖЕ ГОТОВЫМ слотам (см.
                  // LevelContent) — тогда перемешиваем контент под тот же сид, что был при сохранении, ДО применения
                  // позиций из сейва (иначе спавн-точки Soldier/Slime* для applyOptionalMemento ниже указывали бы на
                  // старые места). Без данных о сиде (сейв старее этого поля) оставляем текущую рассадку как есть. Форма
                  // только что могла смениться (shapeChanged) — тогда перемешиваем контент безусловно, даже если сид
                  // содержимого внешне совпадает: старые extraEnemies/itemPickups всё равно относятся к прежним слотам.
                  if (shapeChanged || (memento.hasLevelSeed() && memento.getLevelSeed() != activeContentSeed)) {
                      rerollContent(memento.hasLevelSeed() ? memento.getLevelSeed() : activeContentSeed);
                  }
                  // Полный ребут: убираем всё, что заспавнилось динамически (дети деления слизи, бойцы волны и т.п.) —
                  // старый сейв всё равно ничего о них не знает поимённо, а изначальный ростер ниже сбрасывается отдельно.
                  actorsContainer.destroyTransientChildren();
                  // Волны арены и границы камеры — либо назад на подземелье, либо, если сейв сделан прямо во время боя
                  // на арене, восстанавливает именно ту волну (см. applyArenaWaveMemento выше — раньше откатывало
                  // безусловно, игрок пропадал с экрана).
                  bool restoredIntoArenaWave
                      = applyArenaWaveMemento(arenaWaves, camera, dungeonCameraBounds, arenaCameraBounds, memento);
                  currentLocation = restoredIntoArenaWave ? Location::Arena : Location::Dungeon;

                  if (resetActorsFirst) {
                      playerObject.resetComponents();
                      enemyObject.resetComponents();
                      soldierObject.resetComponents();
                      slimeObject.resetComponents();
                      slime2Object.resetComponents();
                      slime3Object.resetComponents();
                      for (GameObject* extra : extraEnemies) {
                          extra->resetComponents();
                      }
                  }
                  // Ящик со стрелами/сундуки/предметы на карте не входят в сохранение сами по себе — сбрасываются
                  // безусловно в обоих случаях. Предметы — ДО applyInventoryMemento ниже, а не после: тот отдельно
                  // прячет обратно те, что сейв запомнил уже подобранными (см. getCollectedPickups) — обратный порядок
                  // затирал бы это безусловным сбросом здесь (баг: подобранные предметы дублировались в мешке при загрузке).
                  crateObject.resetComponents();
                  resetInteractables();
                  // Сундуки, уже открытые на момент сохранения (см. GameMemento::getOpenedChests) — resetInteractables()
                  // выше вернул все сундуки к закрытому виду безусловно, здесь избирательно возвращаем открытыми те,
                  // что сейв помнит открытыми, БЕЗ повторной выдачи предмета (он уже в bagSlots, см.
                  // applyInventoryMemento ниже) — иначе открыть их заново после загрузки дублировало бы предмет
                  // (баг-дубликат, найден при аудите сохранений). Файлы старее этого поля просто оставляют все
                  // сундуки закрытыми, как было раньше.
                  if (memento.hasOpenedChestsData()) {
                      const std::vector<int>& openedChests = memento.getOpenedChests();
                      std::size_t chestIndex = 0;
                      for (ChestComponent* chest : GameWorld::instance().getRoot().getComponentsInChildren<ChestComponent>()) {
                          if (chestIndex < openedChests.size() && openedChests[chestIndex]) {
                              chest->markOpenedFromSave();
                          }
                          ++chestIndex;
                      }
                  }
                  for (GameObject* pickup : itemPickups) {
                      pickup->resetComponents();
                  }

                  playerObject.setPosition(memento.getPlayerPosition());
                  if (playerHealth) {
                      // Сбрасываем безусловно — иначе загрузка сейва СТАРЕЕ этого поля унаследовала бы streak,
                      // оставшийся в памяти от прежнего забега (например, игрок убил пару ботов, вышел в главное
                      // меню без сохранения, затем загрузил старый сейв без этих данных) вместо честного нуля.
                      dungeonKillStreak = 0;
                      // Максимум ДО текущего HP — setHp() прижимает к [0, maxHp], со старым (базовым) максимумом
                      // сохранённый bonus-hp обрезался бы до него же (баг).
                      if (memento.hasKillStreakData()) {
                          playerHealth->setMaxHp(memento.getPlayerMaxHp());
                          dungeonKillStreak = memento.getDungeonKillStreak();
                      }
                      playerHealth->setHp(memento.getPlayerHp());
                  }
                  enemyObject.setPosition(memento.getEnemyPosition());
                  if (enemyHealth) {
                      enemyHealth->setHp(memento.getEnemyHp());
                  }
                  // Файлы, сохранённые до появления Soldier/Slime(2/3), данных о них не содержат — тогда на точку спавна.
                  applyOptionalMemento(soldierObject, soldierHealth, soldierPosition, memento.hasSoldierData(),
                      memento.getSoldierPosition(), memento.getSoldierHp());
                  applyOptionalMemento(slimeObject, slimeHealth, slimePosition, memento.hasSlimeData(),
                      memento.getSlimePosition(), memento.getSlimeHp());
                  applyOptionalMemento(slime2Object, slime2Health, slime2Position, memento.hasSlime2Data(),
                      memento.getSlime2Position(), memento.getSlime2Hp());
                  applyOptionalMemento(slime3Object, slime3Health, slime3Position, memento.hasSlime3Data(),
                      memento.getSlime3Position(), memento.getSlime3Hp());
                  applyExtraEnemiesMemento(extraEnemies, extraEnemyPositions, memento);
                  // KillStreakComponent видел бы "ожил -> снова умер" (resetComponents() выше оживляет, а setHp()
                  // только что мог вернуть 0 обратно) как настоящее убийство и засчитывал бы награду за уже давно
                  // убитого бота на каждую перезагрузку сейва (был баг — фарм +1 maxHP через "Загрузить
                  // сохранение"). Синхронизируем ПОСЛЕ того, как все HP выше уже приведены к честным, сохранённым
                  // значениям — единым проходом по всем "настоящим" врагам подземелья (см. KillStreakComponent.h).
                  auto resyncKillStreak = [](GameObject& obj) {
                      if (auto* killStreak = obj.getComponent<KillStreakComponent>()) {
                          killStreak->syncToCurrentState();
                      }
                  };
                  resyncKillStreak(enemyObject);
                  resyncKillStreak(soldierObject);
                  resyncKillStreak(slimeObject);
                  resyncKillStreak(slime2Object);
                  resyncKillStreak(slime3Object);
                  for (GameObject* extra : extraEnemies) {
                      resyncKillStreak(*extra);
                  }
                  // Мешок+экипировка игрока, боезапас Soldier, счётчик выстрелов Slime3, "уже подобрано" на карте — файлы
                  // старее инвентаря просто ничего тут не меняют (см. applyInventoryMemento).
                  applyInventoryMemento(playerInventory, soldierAmmo, slime3ShotLimit, itemPickups, memento);
                  // Если сейв восстановлен прямо в волне арены, нужную музыку уже включил applyArenaWaveMemento выше
                  // (см. ArenaWaveComponent::spawnWave) — не затираем её обратно на theme.wav (баг: "музыка волн не
                  // сохраняется в сохранении", музыка подземелья глушила музыку волны сразу же после восстановления).
                  if (!restoredIntoArenaWave) {
                      AudioSystem::instance().playMusic("Resources/Sounds/theme.wav", true);
                  }
                  // Секундомер — восстанавливаем на значение из сейва (см. GameTimerComponent::setElapsed), а не
                  // безусловный reset() на ноль: тот позволял бы занижать честное время в таблице лидеров, просто
                  // пересохранившись прямо перед финишем (баг, найден при проверке сохранения на абьюзы). Файлы
                  // старее этого поля просто обнуляются, как было раньше. Статус "имя для победы уже введено" —
                  // общий хвост всех точек входа в игру, не только "Начать": старый victoryNameEntered=true (если
                  // сохранились уже после какой-то прошлой победы) не дал бы показаться новому экрану ввода имени вовсе.
                  gameTimer.reset();
                  if (memento.hasElapsedTimeData()) {
                      gameTimer.setElapsed(sf::seconds(memento.getElapsedSeconds()));
                  }
                  victoryNameEntered = false;
                  GameWorld::instance().setGameOver(false);
                  GameWorld::instance().setVictory(false);
                  GameWorld::instance().setStarted(true);
                  GameWorld::instance().setPaused(false);
                  // Пока мир на паузе, ни InputComponent, ни WeaponComponent/PlayerAttackComponent/ItemPickupComponent
                  // не polled — клавиша, которой подтвердили этот пункт меню (Enter/Space), иначе на первом кадре
                  // геймплея прочиталась бы как "только что нажали" и выполнила бы рывок/прыжок без реального нового
                  // нажатия (см. resyncPlayerInput выше).
                  resyncPlayerInput();
              };

        // Грузит сохранение.
        mainMenuItems.push_back(
            {"Продолжить", [&loadFromMemento] { loadFromMemento("Продолжить", false); }, [] { return saveFileExists(); }});
        mainMenuItems.push_back(
            {"Начать", [&resyncPlayerInput, &performFullReroll, &playerObject, &playerSpawn, &gameTimer, &victoryNameEntered] {
                 // Рандом всегда, при каждом нажатии — независимо от того, что было до этого (свежий запуск процесса,
                 // возврат из загруженного сейва и т.п.): баг было ровно наоборот — планировка/рассадка "залипала" на
                 // то, что успело загрузиться раньше в этом же процессе (рандом не рандомил локации при
                 // перезапуске уровня — раньше пересобирался только контент, форма подземелья строилась ровно один
                 // раз за весь процесс). performFullReroll — новая форма (какие комнаты/чанки) И новое содержимое.
                 performFullReroll();
                 // playerSpawn мог сместиться вместе с новой формой — игрок к этому моменту уже стоит на СТАРОМ
                 // playerSpawn (см. "В главное меню"/пункты поражения выше, откуда обычно и попадают на этот экран).
                 playerObject.setPosition(playerSpawn);
                 AudioSystem::instance().playMusic("Resources/Sounds/theme.wav", true);
                 // Секундомер и статус "имя для победы уже введено" — общий хвост всех точек входа в игру (см.
                 // GameTimerComponent.h), не только "Начать": иначе, например, после "Продолжить" таймер продолжал бы
                 // идти с того значения, что было в момент сохранения БЕЗ реального игрового времени между ними, а
                 // старый victoryNameEntered=true (если сохранились уже после какой-то прошлой победы) не дал бы
                 // показаться новому экрану ввода имени вовсе.
                 gameTimer.reset();
                 victoryNameEntered = false;
                 // На случай, если сюда попали из ветки, забывшей сбросить эти флаги (см. фикс "В главное меню" на
                 // экране поражения выше) — "Начать" гарантированно должно давать чистое состояние независимо от
                 // истории, так же безусловно, как это уже делают "Продолжить"/"Загрузить сохранение".
                 GameWorld::instance().setGameOver(false);
                 GameWorld::instance().setVictory(false);
                 GameWorld::instance().setStarted(true);
                 GameWorld::instance().setPaused(false);
                 resyncPlayerInput();
             }});
        mainMenuItems.push_back({"Список лидеров", [&showLeaderboard] { showLeaderboard(); }});
        mainMenuItems.push_back({"Настройки", [&settings] { settings.show(); }});
        mainMenuItems.push_back({"Помощь", [&help] { help.show(); }});
        mainMenuItems.push_back({"Титры", [&credits] { credits.show(); }});
        mainMenuItems.push_back({"Выход", [] { Engine::instance().stop(); }});
        mainMenuObject->addComponent<MenuOverlayComponent>(
            sf::Vector2f((float)windowWidth, (float)windowHeight), "Resources/GUI/Panel_9Slice_A.png",
            "Resources/GUI/Button_A_Sheet.png", "Resources/Fonts/Roboto-Bold.ttf", "ROGALIQUE", std::move(mainMenuItems),
            [] { return !GameWorld::instance().hasStarted(); }, "ui_move", "ui_confirm");
        GameWorld::instance().getUIRoot().addChild(std::move(mainMenuObject));
        LOG_INFO("UI: главное меню добавлено");

        // Слушает Escape и переключает паузу.
        auto pauseToggleObject = std::make_unique<GameObject>(sf::Vector2f(0.f, 0.f));
        pauseToggleObject->addComponent<PauseToggleComponent>([&resyncPlayerInput] { resyncPlayerInput(); });
        GameWorld::instance().getUIRoot().addChild(std::move(pauseToggleObject));

        // Экран инвентаря — целиком engine-side (см. класс-комментарий InventoryOverlayComponent.h). Раньше этот
        // класс жил здесь, в Rogalique.exe, и хранил SFML-объекты как поля — из-за двух независимых статических
        // копий SFML (Engine.dll и Rogalique.exe обе линкуют SFML_STATIC) это ломало рендер всего окна намертво.
        // Теперь класс ничего не знает про ItemDefinition/InventoryComponent — только про generic-слоты
        // (InventorySlotView), а связь с реальным инвентарём игрока держат лямбды ниже.
        if (playerInventory) {
            std::vector<std::string> equipFramePaths;
            for (const char* frameName : EQUIP_FRAME_NAMES) {
                equipFramePaths.push_back(std::string("Resources/GUI/Frame_") + frameName + ".png");
            }
            auto inventoryOverlayObject = std::make_unique<GameObject>(sf::Vector2f(0.f, 0.f));
            inventoryOverlayObject->addComponent<InventoryOverlayComponent>(
                sf::Vector2f((float)windowWidth, (float)windowHeight), "Resources/GUI/Panel_9Slice_A.png",
                "Resources/GUI/Frame_Empty.png", "Resources/Fonts/Roboto-Bold.ttf", "ИНВЕНТАРЬ", "ЭКИПИРОВКА", "МЕШОК",
                InventoryComponent::BAG_SIZE, std::move(equipFramePaths),
                [playerInventory](int index) -> InventorySlotView {
                    const InventorySlot& slot = playerInventory->getBag()[index];
                    if (slot.isEmpty()) {
                        return InventorySlotView{};
                    }
                    return InventorySlotView{slot.item->iconPath, slot.item->iconFrameCount, slot.count};
                },
                [playerInventory](int index) -> InventorySlotView {
                    ItemCategory category = EQUIP_CATEGORY_ORDER[index];
                    const ItemDefinition* item = playerInventory->getEquipped(category);
                    if (!item) {
                        return InventorySlotView{};
                    }
                    // count здесь — не стек (экипировка не стакается), а оставшиеся заряды прочности; 0 у
                    // Ring/Neck/Weapon скрывает число совсем (см. InventoryOverlayComponent::refreshVisuals).
                    return InventorySlotView{item->iconPath, item->iconFrameCount, playerInventory->getDurability(category)};
                },
                [playerInventory](int index) { playerInventory->useBagSlot(index); },
                [playerInventory](int index) { playerInventory->unequip(EQUIP_CATEGORY_ORDER[index]); },
                [&resyncPlayerInput] { resyncPlayerInput(); });
            GameWorld::instance().getUIRoot().addChild(std::move(inventoryOverlayObject));
            LOG_INFO("UI: экран инвентаря добавлен");
        }

        // Меню паузы добавлено в UI дерево перед хотбаром HP.
        auto pauseMenuObject = std::make_unique<GameObject>(sf::Vector2f(0.f, 0.f));
        std::vector<MenuOverlayComponent::Item> pauseItems;
        pauseItems.push_back({"Продолжить", [&resyncPlayerInput] {
                                  GameWorld::instance().setPaused(false);
                                  resyncPlayerInput();
                              }});
        pauseItems.push_back({"Сохранить",
            [&playerObject, &enemyObject, &soldierObject, &slimeObject, &slime2Object, &slime3Object, &itemPickups,
                &activeContentSeed, &activeShapeSeed, playerHealth, enemyHealth, soldierHealth, slimeHealth, slime2Health,
                slime3Health, playerInventory, soldierAmmo, slime3ShotLimit, &extraEnemies, &arenaWaves, &dungeonKillStreak,
                &gameTimer] {
                GameMemento memento;
                memento.setPlayerPosition(playerObject.getPosition());
                memento.setPlayerHp(playerHealth ? playerHealth->getHp() : 0);
                memento.setEnemyPosition(enemyObject.getPosition());
                memento.setEnemyHp(enemyHealth ? enemyHealth->getHp() : 0);
                memento.setSoldierPosition(soldierObject.getPosition());
                memento.setSoldierHp(soldierHealth ? soldierHealth->getHp() : 0);
                memento.setSlimePosition(slimeObject.getPosition());
                memento.setSlimeHp(slimeHealth ? slimeHealth->getHp() : 0);
                memento.setSlime2Position(slime2Object.getPosition());
                memento.setSlime2Hp(slime2Health ? slime2Health->getHp() : 0);
                memento.setSlime3Position(slime3Object.getPosition());
                memento.setSlime3Hp(slime3Health ? slime3Health->getHp() : 0);
                if (playerInventory) {
                    std::vector<GameMemento::BagSlotSave> bagSave;
                    for (const InventorySlot& slot : playerInventory->getBag()) {
                        GameMemento::BagSlotSave save;
                        if (!slot.isEmpty()) {
                            save.itemId = slot.item->id;
                            save.count = slot.count;
                        }
                        bagSave.push_back(std::move(save));
                    }
                    std::vector<GameMemento::EquipSlotSave> equipSave;
                    for (ItemCategory category : EQUIP_CATEGORY_ORDER) {
                        GameMemento::EquipSlotSave save;
                        save.category = static_cast<int>(category);
                        const ItemDefinition* item = playerInventory->getEquipped(category);
                        if (item) {
                            save.itemId = item->id;
                            save.durability = playerInventory->getDurability(category);
                        }
                        equipSave.push_back(std::move(save));
                    }
                    std::vector<int> collectedPickups;
                    for (GameObject* pickup : itemPickups) {
                        auto* pickupComponent = pickup->getComponent<ItemPickupComponent>();
                        collectedPickups.push_back(pickupComponent && pickupComponent->isCollected() ? 1 : 0);
                    }
                    memento.setInventoryData(std::move(bagSave), std::move(equipSave), soldierAmmo ? soldierAmmo->getArrows() : 0,
                        slime3ShotLimit ? slime3ShotLimit->getShotsFired() : 0, std::move(collectedPickups));
                }
                memento.setLevelSeed(activeContentSeed);
                std::vector<GameMemento::ExtraEnemySave> extraSave;
                extraSave.reserve(extraEnemies.size());
                for (GameObject* extra : extraEnemies) {
                    GameMemento::ExtraEnemySave save;
                    save.position = extra->getPosition();
                    auto* health = extra->getComponent<HealthComponent>();
                    save.hp = health ? health->getHp() : 0;
                    extraSave.push_back(save);
                }
                memento.setExtraEnemies(std::move(extraSave));
                memento.setLevelShapeSeed(activeShapeSeed);
                // -1, если волны ещё не запущены (обычный бой в подземелье) — см. applyArenaWaveMemento в загрузке.
                memento.setArenaWave(arenaWaves.getCurrentWave());
                memento.setKillStreakData(playerHealth ? playerHealth->getMaxHp() : 0, dungeonKillStreak);
                memento.setElapsedTime(gameTimer.getElapsed().asSeconds());
                // По тому же порядку обхода, что и resetInteractables()/loadFromMemento ниже — предмет из
                // сундука уже осел в bagSave выше, без этого сундук при загрузке открывался бы заново и выдавал
                // его повторно (баг-дубликат, найден при аудите сохранений).
                std::vector<int> openedChests;
                for (ChestComponent* chest : GameWorld::instance().getRoot().getComponentsInChildren<ChestComponent>()) {
                    openedChests.push_back(chest->isOpened() ? 1 : 0);
                }
                memento.setOpenedChests(std::move(openedChests));
                if (!memento.save(savePath())) {
                    LOG_WARN("Сохранить: не удалось записать файл сохранения");
                }
            }});
        pauseItems.push_back({"Настройки", [&settings] { settings.show(); }});
        pauseItems.push_back({"Помощь", [&help] { help.show(); }});

        // "В главное меню" — общий хвост для пауз/поражения/победы (см. три места использования ниже): раньше
        // был продублирован в каждом почти дословно (~35 строк x3, ~110 строк copy-paste) — именно в одной из
        // этих трёх копий (у экрана поражения) жил баг "isGameOver() не сбрасывался" (см. фикс выше в этой же
        // сессии): пункт "Загрузить сохранение" сбрасывал флаг явно, а "В главное меню" рядом — по недосмотру нет,
        // и синхронизировать несколько копий вручную оказалось ненадёжно. Сбрасывает ОБА флага (setGameOver И
        // setVictory) безусловно, даже если конкретный вызывающий экран не мог бы выставить какой-то из них —
        // это не ошибка, а защита именно от такого класса бага на будущее: сброс уже false-флага — no-op.
        auto returnToMainMenu
            = [&playerObject, &enemyObject, &soldierObject, &crateObject, &slimeObject, &slime2Object, &slime3Object,
                  &actorsContainer, &itemPickups, &playerSpawn, &enemyPosition, &soldierPosition, &slimePosition, &slime2Position,
                  &slime3Position, &extraEnemies, &extraEnemyPositions, &arenaWaves, camera, &dungeonCameraBounds,
                  &currentLocation] {
                  actorsContainer.destroyTransientChildren();
                  // Волны арены (см. ArenaWaveComponent) — тот же принцип "полного ребута", что и у остального выше:
                  // новая игра/выход в меню не должна помнить, что где-то по пути игрок уже прошёл часть волн.
                  arenaWaves.reset();
                  currentLocation = Location::Dungeon;
                  // Границы камеры — назад на подземелье (см. dungeonCameraBounds/arenaCameraBounds выше): игрок
                  // вернётся на playerSpawn в подземелье, а не останется с границами арены.
                  if (camera) {
                      camera->setBounds(dungeonCameraBounds);
                  }
                  resetToSpawn(playerObject, playerSpawn);
                  resetToSpawn(enemyObject, enemyPosition);
                  resetToSpawn(soldierObject, soldierPosition);
                  resetToSpawn(slimeObject, slimePosition);
                  resetToSpawn(slime2Object, slime2Position);
                  resetToSpawn(slime3Object, slime3Position);
                  for (std::size_t i = 0; i < extraEnemies.size(); ++i) {
                      resetToSpawn(*extraEnemies[i], extraEnemyPositions[i]);
                  }
                  crateObject.resetComponents();
                  for (GameObject* pickup : itemPickups) {
                      pickup->resetComponents();
                  }
                  // Сундуки (см. Chest) живут в levelContainer, не в actors/itemPickups — сбрасываем отдельно тем же
                  // приёмом поиска по компоненту, что и resyncPlayerInput ниже (баг: без этого открытый сундук оставался
                  // открытым насовсем до конца процесса, даже после "Начать" заново).
                  resetInteractables();
                  // Победа/поражение могли последними включить win.wav/сохранить gameover-тишину — возврат в главное
                  // меню всегда переключает музыку обратно, независимо от того, откуда сюда попали.
                  AudioSystem::instance().playMusic("Resources/Sounds/mainmenu.wav", true);
                  GameWorld::instance().setGameOver(false);
                  GameWorld::instance().setVictory(false);
                  GameWorld::instance().setStarted(false);
                  // Иначе мир, разблокированный экраном поражения (см. PlayerDeathComponent.h — специально не на паузе,
                  // чтобы доиграть анимацию смерти), продолжал бы тикать и дальше, пока висит главное меню: враги успевали
                  // снова догнать и убить только что восстановленного игрока, а предметы — тут же подобраться заново, ещё
                  // до нажатия "Начать" (баг: подобранные предметы "пропадали" при новой игре — на деле их успевали подобрать
                  // второй раз сами, вне поля зрения).
                  GameWorld::instance().setPaused(true);
              };

        // Титры нарочно только в главном меню, не в паузе.
        // Не выход из приложения (тот остался только в главном меню) возврат к нему же.
        pauseItems.push_back({"В главное меню", [&returnToMainMenu] { returnToMainMenu(); }});
        pauseMenuObject->addComponent<MenuOverlayComponent>(
            sf::Vector2f((float)windowWidth, (float)windowHeight), "Resources/GUI/Panel_9Slice_A.png",
            "Resources/GUI/Button_A_Sheet.png", "Resources/Fonts/Roboto-Bold.ttf", "ПАУЗА", std::move(pauseItems),
            // !isModalOpen() — иначе меню паузы показывалось бы поверх любого модального оверлея, который тоже
            // ставит игру на паузу (например, InventoryOverlayComponent): его isPaused()==true само по себе
            // делало пункты этого меню видимыми и кликабельными прямо под инвентарём, друг на друге.
            [] {
                return GameWorld::instance().hasStarted() && GameWorld::instance().isPaused()
                       && !GameWorld::instance().isGameOver() && !GameWorld::instance().isVictory()
                       && !GameWorld::instance().isModalOpen();
            },
            "ui_move", "ui_confirm");
        GameWorld::instance().getUIRoot().addChild(std::move(pauseMenuObject));
        LOG_INFO("UI: меню паузы добавлено");

        // Следит за HP игрока и выставляет GameWorld::isGameOver() — мир при этом не останавливаем, см. комментарий
        // в PlayerDeathComponent.h.
        auto playerDeathObject = std::make_unique<GameObject>(sf::Vector2f(0.f, 0.f));
        playerDeathObject->addComponent<PlayerDeathComponent>(playerHealth);
        GameWorld::instance().getUIRoot().addChild(std::move(playerDeathObject));

        // Экран поражения — поверх меню паузы, кнопки грузят последнее сохранение либо возвращают в главное меню.
        auto deathMenuObject = std::make_unique<GameObject>(sf::Vector2f(0.f, 0.f));
        std::vector<MenuOverlayComponent::Item> deathItems;
        // Тот же общий loadFromMemento, что и у "Продолжить" (см. её объявление выше) — resetActorsFirst=true,
        // потому что сюда приходят с экрана поражения (актёры могли остаться в "мёртвом" состоянии, нужен полный
        // сброс компонентов до применения сейва, см. комментарий у loadFromMemento).
        deathItems.push_back({"Загрузить сохранение", [&loadFromMemento] { loadFromMemento("Загрузить сохранение", true); },
            [] { return saveFileExists(); }});
        // Тот же общий хвост, что и у паузы выше (returnToMainMenu) — теперь без риска забыть здесь одну из
        // синхронизированных вручную копий (см. её комментарий, откуда взялся баг с isGameOver()).
        deathItems.push_back({"В главное меню", [&returnToMainMenu] { returnToMainMenu(); }});
        deathMenuObject->addComponent<MenuOverlayComponent>(
            sf::Vector2f((float)windowWidth, (float)windowHeight), "Resources/GUI/Panel_9Slice_A.png",
            "Resources/GUI/Button_A_Sheet.png", "Resources/Fonts/Roboto-Bold.ttf", "ПОРАЖЕНИЕ", std::move(deathItems),
            [] { return GameWorld::instance().isGameOver(); }, "ui_move", "ui_confirm", 24.f);
        GameWorld::instance().getUIRoot().addChild(std::move(deathMenuObject));
        LOG_INFO("UI: экран поражения добавлен");

        // Экран победы — открывается по Door/DoorComponent (см. выше), тот же принцип, что у экрана поражения: мир
        // не на паузе сам по себе (см. комментарий у GameWorld::isVictory), просто оверлей поверх. "Играть заново" —
        // та же логика, что у "Начать" в главном меню (новый сид, новая рассадка); "В главное меню" — та же логика,
        // что у симметричного пункта экрана поражения выше.
        auto victoryMenuObject = std::make_unique<GameObject>(sf::Vector2f(0.f, 0.f));
        std::vector<MenuOverlayComponent::Item> victoryItems;
        victoryItems.push_back(
            {"Играть заново", [&resyncPlayerInput, &performFullReroll, &playerObject, &playerSpawn, &arenaWaves, camera,
                                  &dungeonCameraBounds, &actorsContainer, &gameTimer, &victoryNameEntered] {
                 // Трупы бойцов последней волны (см. TransientComponent у ArenaWaveComponent::spawnEnemy) и любые
                 // недобитые дети деления слизи всё ещё сидят в actorsContainer — обычно их сметает "В главное меню",
                 // но этот пункт идёт из победы напрямую в новую игру, минуя тот путь (баг: без этого явного вызова
                 // они остались бы висеть в дереве сцены до конца процесса, в старых, уже отвязанных от новой формы
                 // уровня координатах).
                 actorsContainer.destroyTransientChildren();
                 // Игрок физически стоит на арене волн (см. как сюда попадают — только через победу над всеми волнами)
                 // — та геометрия сейчас будет снесена и пересобрана заново вместе со всем подземельем (см.
                 // rebuildLevelGeometry внутри performFullReroll — арена тоже её часть, её позиция зависит от ширины
                 // подземелья), поэтому сначала телепортируем игрока обратно в (новый) playerSpawn и возвращаем камеру
                 // на подземелье — иначе игрок остался бы висеть в мировых координатах, где арены уже нет.
                 performFullReroll();
                 arenaWaves.reset();
                 if (camera) {
                     camera->setBounds(dungeonCameraBounds);
                 }
                 playerObject.setPosition(playerSpawn);
                 AudioSystem::instance().playMusic("Resources/Sounds/theme.wav", true);
                 // Секундомер и статус "имя для победы уже введено" — общий хвост всех точек входа в игру (см.
                 // GameTimerComponent.h), не только "Начать": иначе, например, после "Продолжить" таймер продолжал бы
                 // идти с того значения, что было в момент сохранения БЕЗ реального игрового времени между ними, а
                 // старый victoryNameEntered=true (если сохранились уже после какой-то прошлой победы) не дал бы
                 // показаться новому экрану ввода имени вовсе.
                 gameTimer.reset();
                 victoryNameEntered = false;
                 GameWorld::instance().setVictory(false);
                 GameWorld::instance().setStarted(true);
                 GameWorld::instance().setPaused(false);
                 resyncPlayerInput();
             }});
        // Тот же общий хвост, что и у паузы/поражения выше (returnToMainMenu) — победа только что играла win.wav
        // (см. ArenaWaveComponent::onAllWavesCleared), returnToMainMenu переключает музыку обратно так же, как и
        // из двух других экранов.
        victoryItems.push_back({"В главное меню", [&returnToMainMenu] { returnToMainMenu(); }});
        victoryMenuObject->addComponent<MenuOverlayComponent>(
            sf::Vector2f((float)windowWidth, (float)windowHeight), "Resources/GUI/Panel_9Slice_A.png",
            "Resources/GUI/Button_A_Sheet.png", "Resources/Fonts/Roboto-Bold.ttf", "ПОБЕДА", std::move(victoryItems),
            // Не сразу вместе с isVictory() — сначала должен закрыться экран ввода имени (см. nameEntryOverlay/
            // victoryNameEntered выше), иначе оба оверлея были бы видны и кликабельны одновременно.
            [&victoryNameEntered] { return GameWorld::instance().isVictory() && victoryNameEntered; }, "ui_move", "ui_confirm",
            24.f);
        GameWorld::instance().getUIRoot().addChild(std::move(victoryMenuObject));
        // Плавное появление экрана победы (был "слишком резко появляется") — та же триггер-функция,
        // что у самого экрана, добавлен ПОСЛЕ него, поэтому рисуется поверх и первые полсекунды закрывает его
        // сплошным чёрным, растворяясь в прозрачность.
        auto victoryFadeObject = std::make_unique<GameObject>(sf::Vector2f(0.f, 0.f));
        victoryFadeObject->addComponent<ScreenFadeComponent>(sf::Vector2f((float)windowWidth, (float)windowHeight),
            sf::Color::Black, sf::seconds(0.6f),
            [&victoryNameEntered] { return GameWorld::instance().isVictory() && victoryNameEntered; });
        GameWorld::instance().getUIRoot().addChild(std::move(victoryFadeObject));
        LOG_INFO("UI: экран победы добавлен");

        // Титры, помощь, список лидеров и настройки  — это модальные экраны поверх любого меню.
        GameWorld::instance().getUIRoot().addChild(std::move(creditsObject));
        LOG_INFO("UI: экран титров добавлен");
        GameWorld::instance().getUIRoot().addChild(std::move(helpObject));
        LOG_INFO("UI: экран помощи добавлен");
        GameWorld::instance().getUIRoot().addChild(std::move(leaderboardObject));
        LOG_INFO("UI: экран списка лидеров добавлен");
        GameWorld::instance().getUIRoot().addChild(std::move(settingsObject));
        LOG_INFO("UI: экран настроек добавлен");

        // HUD.
        if (playerHealth) {
            GameObject& uiRoot = GameWorld::instance().getUIRoot();
            auto playerBar = std::make_unique<GameObject>(sf::Vector2f(16.f, 16.f));
            playerBar->addComponent<HealthBarComponent>(*playerHealth, "Resources/GUI/Bar_C.png", sf::Vector2f(231.f, 48.f),
                [] { return GameWorld::instance().hasStarted(); });
            uiRoot.addChild(std::move(playerBar));

            // Виньетирование по краям экрана на "мало здоровья" — тот же порог и тот же период пульсации, что и у
            // LowHealthPulseComponent на теле игрока, читает те же Player::LOW_HP_THRESHOLD/LOW_HP_PULSE_PERIOD
            // (см. Player.h), а не собственную копию — раньше это были два независимых хардкод-литерала, и
            // рассинхрон периода между телом и экраном уже был багом (см. аудит дублирования кода).
            // !isPaused() — иначе компонент висит в uiRoot (тот, в отличие от m_root, тикает и на паузе) и продолжал
            // бы пульсировать поверх меню паузы/инвентаря, что тоже смущало игрока.
            auto lowHpFlash = std::make_unique<GameObject>(sf::Vector2f(0.f, 0.f));
            lowHpFlash->addComponent<LowHealthScreenFlashComponent>(*playerHealth,
                sf::Vector2f((float)windowWidth, (float)windowHeight), Player::LOW_HP_THRESHOLD, Player::LOW_HP_PULSE_PERIOD,
                [] { return GameWorld::instance().hasStarted() && !GameWorld::instance().isPaused(); });
            uiRoot.addChild(std::move(lowHpFlash));
        }
        if (playerInput) {
            // Полоска выносливости — под полоской HP (та занимает 16,16 размером 231x48), тем же приёмом, что и
            // ArmorBadgeComponent ниже: своя строка HUD, не перекрывается с остальными.
            auto staminaBar = std::make_unique<GameObject>(sf::Vector2f(16.f, 16.f + 48.f + 4.f));
            staminaBar->addComponent<FractionBarComponent>(
                [playerInput] { return playerInput->getStamina() / InputComponent::STAMINA_MAX; },
                sf::Vector2f(140.f, 10.f), sf::Color(20, 20, 20, 200), sf::Color(90, 200, 235, 235),
                [] { return GameWorld::instance().hasStarted(); });
            GameWorld::instance().getUIRoot().addChild(std::move(staminaBar));
        }
        if (playerInventory) {
            // Бейдж суммарной прочности надетой ломающейся брони (Щит/Нагрудник/Штаны/Шлем) — броня теперь
            // работает как вторая шкала HP (InventoryComponent::absorbHit), просто число очков, без полоски/доли:
            // с полоской "исторический максимум" визуально скакал то на пустую, то путал долю при поломке одной из
            // частей — простое число этого не делает и не нуждается в скрытии в каком-либо состоянии, даже "0".
            // Под полоской здоровья И полоской стамины (16,68 размером 140x10) — своя строка HUD, ниже обеих.
            auto armorBadge = std::make_unique<GameObject>(sf::Vector2f(16.f, 16.f + 48.f + 10.f + 8.f));
            armorBadge->addComponent<ArmorBadgeComponent>([playerInventory] { return playerInventory->getTotalDurability(); },
                "Resources/Map/Shield.png", "Resources/Fonts/Roboto-Bold.ttf", [] { return GameWorld::instance().hasStarted(); });
            GameWorld::instance().getUIRoot().addChild(std::move(armorBadge));
        }
        if (playerWeapon) {
            // Текущее оружие + патроны пистолета (см. HudTextComponent — генерик-лейбл, вся логика строки здесь).
            auto weaponHud = std::make_unique<GameObject>(sf::Vector2f((float)windowWidth - 16.f, 16.f));
            weaponHud->addComponent<HudTextComponent>(
                "Resources/Fonts/Roboto-Bold.ttf", 18, sf::Color::White,
                [playerWeapon]() -> std::string {
                    switch (playerWeapon->getCurrent()) {
                    case Weapon::Spear:
                        return "Копьё";
                    case Weapon::Gun: {
                        if (playerWeapon->isReloading()) {
                            return "Арбалет: перезарядка...";
                        }
                        return "Арбалет: " + std::to_string(playerWeapon->getAmmo()) + "/"
                               + std::to_string(playerWeapon->getReserveAmmo());
                    }
                    default:
                        return "Без оружия";
                    }
                },
                true, [] { return GameWorld::instance().hasStarted(); });
            GameWorld::instance().getUIRoot().addChild(std::move(weaponHud));
        }
        if (playerInventory && playerWeapon) {
            // Подсказки управления — часть про Shift/Ctrl растёт по мере находок экипировки (Сапоги/Кольцо), как
            // и раньше; боевая строка (R/F) зависит от того, что сейчас в руках (см. WeaponComponent::getCurrent),
            // а не от того, что вообще есть — с копьём R не нажать, с арбалетом "F — атака" звучало бы не в кассу.
            // Tab/E — всегда, это не находки, а базовое управление.
            auto hintsHud = std::make_unique<GameObject>(sf::Vector2f((float)windowWidth - 16.f, (float)windowHeight - 90.f));
            hintsHud->addComponent<HudTextComponent>(
                "Resources/Fonts/Roboto-Regular.ttf", 15, sf::Color(220, 220, 220),
                [playerInventory, playerWeapon]() -> std::string {
                    std::vector<std::string> lines;
                    if (playerInventory->getEquipped(ItemCategory::Boots)) {
                        lines.push_back("Shift — спринт");
                    }
                    if (playerInventory->getEquipped(ItemCategory::Ring)) {
                        lines.push_back("Ctrl — рывок");
                    }
                    switch (playerWeapon->getCurrent()) {
                    case Weapon::Gun:
                        lines.push_back("R — перезарядка");
                        lines.push_back("F / ЛКМ — выстрел");
                        break;
                    case Weapon::Spear:
                        lines.push_back("F / ЛКМ — атака");
                        break;
                    default:
                        break; // Безоружен — бить нечем, боевой строки нет.
                    }
                    lines.push_back("Q / колесо — смена оружия");
                    lines.push_back("Tab / ПКМ — инвентарь");
                    lines.push_back("E — взаимодействие");
                    std::string text;
                    for (std::size_t i = 0; i < lines.size(); ++i) {
                        text += lines[i];
                        if (i + 1 < lines.size()) {
                            text += "\n";
                        }
                    }
                    return text;
                },
                true, [] { return GameWorld::instance().hasStarted(); });
            GameWorld::instance().getUIRoot().addChild(std::move(hintsHud));
        }
        // Таймер прохождения — низ слева. Сам счётчик — GameTimerComponent выше, эта HudTextComponent
        // только рисует его текущее значение, тем же приёмом, что и остальной HUD (оружие/подсказки справа).
        auto timerHud = std::make_unique<GameObject>(sf::Vector2f(16.f, (float)windowHeight - 32.f));
        timerHud->addComponent<HudTextComponent>(
            "Resources/Fonts/Roboto-Bold.ttf", 20, sf::Color::White,
            [&gameTimer]() -> std::string { return formatElapsedTime(gameTimer.getElapsed()); }, false,
            [] { return GameWorld::instance().hasStarted(); });
        GameWorld::instance().getUIRoot().addChild(std::move(timerHud));

        // Gameplay: текущая локация (см. Location.h) — прямо над таймером, чтобы переход подземелье/арена был
        // заметен и объясним (см. docs/DESIGN_DOC.md), а не просто угадывался по смене фона/музыки.
        auto locationHud = std::make_unique<GameObject>(sf::Vector2f(16.f, (float)windowHeight - 52.f));
        locationHud->addComponent<HudTextComponent>(
            "Resources/Fonts/Roboto-Bold.ttf", 16, sf::Color(220, 220, 220),
            [&currentLocation]() -> std::string { return locationDisplayName(currentLocation); }, false,
            [] { return GameWorld::instance().hasStarted(); });
        GameWorld::instance().getUIRoot().addChild(std::move(locationHud));

        // Всплывающие уведомления (подобранные предметы, запертая дверь и т.п.) — один экземпляр на весь UI, сам
        // компонент только рисует (см. ToastNotificationSystem.h — очередь и таймер там, показывается независимо
        // от hasStarted(), чтобы не потерять уведомление, если оно всплыло прямо на грани паузы/меню).
        auto toastObject = std::make_unique<GameObject>(sf::Vector2f(0.f, 0.f));
        toastObject->addComponent<ToastNotificationComponent>(
            sf::Vector2f((float)windowWidth, (float)windowHeight), "Resources/Fonts/Roboto-Bold.ttf");
        GameWorld::instance().getUIRoot().addChild(std::move(toastObject));

        LOG_INFO("HUD: полоска HP героя, стамина, прочность брони, оружие, подсказки, таймер и уведомления добавлены");

        // Engine::instance().run() (сам игровой цикл) — ВНУТРИ этого try, а не после него: почти все обработчики
        // пунктов меню выше (Item::onActivate) держат ссылки на локальные переменные этой функции (playerObject,
        // itemPickups, activeContentSeed и т.д.) — если бы run() вызывался уже ПОСЛЕ закрывающей скобки try, эти
        // локальные объекты формально вышли бы из области видимости раньше, чем игровой цикл вообще успел бы их
        // использовать: для классов с деструктором (например, std::vector<GameObject*> itemPickups) это значит
        // реальный вызов деструктора на границе scope — стек ещё не переиспользован (мы всё ещё внутри кадра этой
        // же функции), но объект уже приведён в пустое состояние, и все ссылки на него становятся use-after-destruction.
        // Для POD (int/sf::Vector2f, без деструктора) эффект не проявляется — значение просто остаётся как было,
        // отсюда и разброс: часть "сломанного" состояния молчала, пока не завёлся Item, реально зависящий от
        // vector-поля (см. коммит с багом "сейв не помнил уже подобранные предметы").
        Engine::instance().run();
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("SceneFacade: критическая ошибка при сборке сцены или во время работы игры: ") + e.what());
    }
}
