#pragma once
#include <SFML/System/Time.hpp>
#include <string>

// Категория предмета: Consumable/Misc — предмет только лежит в мешке. Остальные — под конкретный слот
// экипировки (см. Resources/GUI/Frame_*.png) и одновременно ключ в InventoryComponent для "что сейчас надето".
enum class ItemCategory { Consumable, Misc, Weapon, Head, Chest, Boots, Ring, Neck, Shield, Pants };

// Статичное описание предмета (не экземпляр в мешке) — на него ссылаются по указателю (см. InventorySlot), не
// копируют. Живёт в статическом реестре внутри ItemDefinition.cpp, поэтому указатель, полученный через
// findItemDefinition(), валиден всё время жизни игры.
struct ItemDefinition {
    std::string id;
    std::string displayName;
    ItemCategory category;
    std::string iconPath;
    int iconFrameCount = 1;
    sf::Time iconFrameDuration = sf::Time::Zero;
    int maxStack = 1;
    // Consumable — сколько HP восстанавливает при использовании (см. InventoryComponent::useBagSlot). 0 у
    // остальных категорий.
    int healAmount = 0;
    // Экипируемые категории — постоянный бонус к броне, пока предмет надет (см.
    // InventoryComponent::recomputeEquipmentEffects). 0 у Consumable/Misc и у брони с прочностью (см. durability
    // ниже) — та защищает иначе, не суммируемым стат-бонусом.
    int armorBonus = 0;
    // Прочность в "заряженных" ударах — пока заряды не кончились, надетая часть полностью блокирует урон одного
    // удара (см. InventoryComponent::absorbHit), не одновременно с другими такими частями, а по очереди
    // (DURABILITY_ORDER в InventoryComponent.cpp). Когда заряды кончаются, часть ломается и уничтожается. 0 —
    // у предмета нет этой механики (Weapon/Ring/Neck, либо ещё не нашлось арта под неё).
    int durability = 0;
};

// Реестр всех предметов игры — собран из готовых иконок в Resources/Map/Items и Resources/Map/Shop/Items.
// Слоты экипировки (Weapon/Head/Chest/Boots/Ring/Neck/Shield/Pants) полностью укомплектованы: Shield/Weapon —
// из Resources/Map/Shield.png и Crossbow/crossbow_*.png, остальные шесть — вырезаны из Shikashi's Fantasy Icons
// Pack (Resources/GUI/#2 - Transparent Icons & Drop Shadow.png) в отдельные файлы Resources/Map/Items/equipment/.
//
// Предмет по id — нужен SceneFacade, чтобы расставить конкретные предметы на карте по имени, а не по индексу.
// nullptr, если такого id нет.
const ItemDefinition* findItemDefinition(const std::string& id);
