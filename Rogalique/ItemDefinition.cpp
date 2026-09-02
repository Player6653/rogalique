#include "ItemDefinition.h"
#include <vector>

namespace
{
    // Баг (был): реальный кадр в этих паках — 16px, а не 32px, как считалось раньше ("проверено по размеру файлов"
    // проверяло только размер листа целиком, не сам кадр). При 32px-нарезке в кадр попадали сразу 2 соседних —
    // предмет на иконке визуально "дублировался". 256x32 -> 16 кадров по 16x32, 128x32 -> 8 кадров по 16x32.
    constexpr int ICON_FRAME_COUNT = 16; // Все паки (potions/keys/Skull) — 256x32, 16 кадров 16x32.
    const sf::Time ICON_FRAME_DURATION = sf::seconds(0.12f);

    constexpr int CROSSBOW_FRAME_COUNT = 8; // Resources/Map/Crossbow/crossbow_*.png — 128x32, 8 кадров 16x32.

    const std::vector<ItemDefinition> ITEMS = {
        {"potion_small", "Малое зелье лечения", ItemCategory::Consumable, "Resources/Map/Items/potions/small_red_potion_01.png",
            ICON_FRAME_COUNT, ICON_FRAME_DURATION, 10, 1, 0},
        {"potion_medium", "Зелье лечения", ItemCategory::Consumable, "Resources/Map/Items/potions/medium_red_potion_01.png",
            ICON_FRAME_COUNT, ICON_FRAME_DURATION, 10, 2, 0},
        {"potion_big", "Большое зелье лечения", ItemCategory::Consumable, "Resources/Map/Items/potions/big_red_potion_01.png",
            ICON_FRAME_COUNT, ICON_FRAME_DURATION, 5, 4, 0},
        {"rusty_key", "Ржавый ключ", ItemCategory::Misc, "Resources/Map/Items/keys/key1_01.png", ICON_FRAME_COUNT,
            ICON_FRAME_DURATION, 5, 0, 0},
        {"ancient_skull", "Древний череп", ItemCategory::Misc, "Resources/Map/Shop/Items/Skull/Skull_01.png", ICON_FRAME_COUNT,
            ICON_FRAME_DURATION, 1, 0, 0},
        // Четыре ключа от главной двери в центре Hub (см. Door/DoorComponent, SceneFacade::run()) — по одному в
        // конце пути в каждую из 4 сторон света (см. ChunkAssemblerConfig::keyRoomPaths), rusty_key выше остаётся
        // обычным лутом без привязки к двери. key_first/key_second — более старые id (изначально было 2 ключа, не
        // 4), оставлены как есть ради обратной совместимости со старыми сейвами, но дверь их больше не требует.
        {"key_first", "Ключ Солнца", ItemCategory::Misc, "Resources/Map/Items/keys/key2_01.png", ICON_FRAME_COUNT,
            ICON_FRAME_DURATION, 1, 0, 0},
        {"key_second", "Ключ Луны", ItemCategory::Misc, "Resources/Map/Items/keys/key3_01.png", ICON_FRAME_COUNT,
            ICON_FRAME_DURATION, 1, 0, 0},
        {"key_north", "Северный ключ", ItemCategory::Misc, "Resources/Map/Items/keys/key2_02.png", ICON_FRAME_COUNT,
            ICON_FRAME_DURATION, 1, 0, 0},
        {"key_south", "Южный ключ", ItemCategory::Misc, "Resources/Map/Items/keys/key3_02.png", ICON_FRAME_COUNT,
            ICON_FRAME_DURATION, 1, 0, 0},
        {"key_east", "Восточный ключ", ItemCategory::Misc, "Resources/Map/Items/keys/key1_02.png", ICON_FRAME_COUNT,
            ICON_FRAME_DURATION, 1, 0, 0},
        {"key_west", "Западный ключ", ItemCategory::Misc, "Resources/Map/Items/keys/key1_03.png", ICON_FRAME_COUNT,
            ICON_FRAME_DURATION, 1, 0, 0},
        // Предметы экипировки — иконки вырезаны из Shikashi's Fantasy Icons Pack (см. титры), 32x32 каждая, без
        // анимации (1 кадр). Полный комплект закрывает все 8 категорий из ItemCategory, но не все дают armorBonus:
        // Shield/Head/Chest/Pants — расходуемая прочность (durability, последнее число, см. ItemDefinition.h) вместо
        // постоянного бонуса; Boots/Ring — не про броню вовсе, отпирают спринт/рывок (см.
        // InventoryComponent::recomputeEquipmentEffects); Weapon (арбалет) отпирает пистолет в WeaponComponent;
        // Neck (ожерелье) — пассивная регенерация HP, пока надето (см. InventoryComponent::update).
        {"shield", "Щит", ItemCategory::Shield, "Resources/Map/Shield.png", 1, sf::Time::Zero, 1, 0, 0, 5},
        {"crossbow", "Арбалет", ItemCategory::Weapon, "Resources/Map/Crossbow/crossbow_down.png", CROSSBOW_FRAME_COUNT,
            ICON_FRAME_DURATION, 1, 0, 0, 0},
        {"helmet", "Шлем", ItemCategory::Head, "Resources/Map/Items/equipment/helmet_01.png", 1, sf::Time::Zero, 1, 0, 0, 1},
        {"chestplate", "Нагрудник", ItemCategory::Chest, "Resources/Map/Items/equipment/chest_01.png", 1, sf::Time::Zero, 1, 0, 0,
            3},
        {"pants", "Штаны", ItemCategory::Pants, "Resources/Map/Items/equipment/pants_01.png", 1, sf::Time::Zero, 1, 0, 0, 2},
        {"boots", "Сапоги", ItemCategory::Boots, "Resources/Map/Items/equipment/boots_01.png", 1, sf::Time::Zero, 1, 0, 0, 0},
        {"ring", "Кольцо", ItemCategory::Ring, "Resources/Map/Items/equipment/ring_01.png", 1, sf::Time::Zero, 1, 0, 0, 0},
        {"necklace", "Ожерелье", ItemCategory::Neck, "Resources/Map/Items/equipment/necklace_01.png", 1, sf::Time::Zero, 1, 0, 0,
            0},
    };
} // namespace

const ItemDefinition* findItemDefinition(const std::string& id)
{
    for (const ItemDefinition& item : ITEMS) {
        if (item.id == id) {
            return &item;
        }
    }
    return nullptr;
}
