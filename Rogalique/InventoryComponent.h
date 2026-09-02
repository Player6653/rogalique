#pragma once
#include "IComponent.h"
#include "ItemDefinition.h"
#include <SFML/System/Time.hpp>
#include <array>
#include <map>

struct InventorySlot {
    const ItemDefinition* item = nullptr;
    int count = 0;
    bool isEmpty() const
    {
        return item == nullptr;
    }
};

// Инвентарь игрока: мешок (стек предметов, см. InventorySlot) + отдельная экипировка по категориям
// (Weapon/Head/Chest/Boots/Ring/Neck/Shield/Pants — см. ItemCategory). При каждой экипировке/снятии/сбросе сама
// пересчитывает всё, что зависит от текущей экипировки (см. recomputeEquipmentEffects) — броню владельца, доступ
// к спринту/рывку и доступность пистолета у WeaponComponent. Базовую броню (без бонусов экипировки) держит
// отдельно от того, что сейчас лежит в HealthComponent, чтобы не потерять её при пересчёте.
class InventoryComponent : public IComponent {
public:
    static constexpr int BAG_SIZE = 16;

    explicit InventoryComponent(int baseArmor);

    // Кладёт в мешок count штук: сперва доливает существующий стек того же предмета, иначе занимает первые
    // пустые слоты. false, если поместить некуда (мешок полон подходящими слотами) — вызывающий не должен
    // считать предмет подобранным.
    bool addItem(const ItemDefinition& item, int count = 1);

    // Убирает count штук предмета с данным id из мешка (по всем подходящим слотам, если предмет почему-то
    // размазан по нескольким стекам) — нужна двери, чтобы забрать ключи при открытии (см. DoorComponent), а не
    // оставлять их бесполезно висеть в мешке до конца забега. true, если реально нашлось и убрано хотя бы что-то.
    bool removeItemById(const std::string& itemId, int count = 1);

    // index — индекс слота мешка (0..BAG_SIZE-1). Consumable — восстанавливает HP владельца и списывает 1 шт.
    // Экипируемая категория — надевает: прежний предмет той же категории (если был) уходит в мешок на
    // освободившееся место. Misc или пустой слот — ничего не делает.
    void useBagSlot(int index);
    // Снимает экипировку категории обратно в мешок, если там есть место. У брони с прочностью (см.
    // ItemDefinition::durability) сбрасывает накопленные повреждения — предмет в мешке снова "как новый", это
    // осознанное упрощение (полноценный учёт прочности отдельно для каждого экземпляра в мешке не реализован).
    void unequip(ItemCategory category);
    // Напрямую сажает предмет в слот экипировки, минуя обычную логику "взять из мешка" (useBagSlot) — нужно
    // только загрузке сохранения (см. SceneFacade), которая восстанавливает уже надетое, а не эмулирует клик по
    // мешку. Прежний предмет в category (если был) просто затирается, не возвращается в мешок — вызывающий код
    // сам отвечает за то, что перед восстановлением сейва инвентарь уже пуст (см. reset()). Не пересчитывает
    // эффекты сама — вызывающий код зовёт recomputeEquipmentEffects() один раз после того, как заполнит все слоты.
    void forceEquip(ItemCategory category, const ItemDefinition& item, int durability);

    const std::array<InventorySlot, BAG_SIZE>& getBag() const
    {
        return m_bag;
    }
    // nullptr, если в этой категории сейчас ничего не надето.
    const ItemDefinition* getEquipped(ItemCategory category) const;
    // Оставшиеся заряды прочности надетой в category брони; 0 и если ничего не надето, и если у надетого предмета
    // durability изначально 0 (Ring/Neck/Weapon) — то есть "0" не значит "вот-вот сломается", см. getEquipped().
    int getDurability(ItemCategory category) const;

    // Суммарно по ВСЕЙ надетой ломающейся броне разом (Щит+Нагрудник+Штаны+Шлем, что из них сейчас надето) — для
    // HUD-бейджа (см. ArmorBadgeComponent в SceneFacade): просто текущее число очков прочности, без доли/максимума.
    int getTotalDurability() const;

    // Поглощает урон надетой бронёй с прочностью — броня работает как вторая шкала HP (см. absorbHit() в .cpp):
    // тратятся именно очки, равные нанесённому урону, а не "один заряд за удар" независимо от его силы. Возвращает
    // остаток урона, который броня не смогла поглотить целиком (0, если поглощено всё) — он идёт дальше в обычную
    // броню/HP (см. HealthComponent::takeDamage). Не вызывается изнутри самого InventoryComponent — порядок
    // добавления компонентов на GameObject не гарантирует, что HealthComponent уже готов на момент конструирования,
    // поэтому подписку делает SceneFacade через HealthComponent::setDamageInterceptor().
    int absorbHit(int incomingDamage);

    // Пересчитывает всё, что зависит от текущей экипировки: броню (HealthComponent::setArmor), доступность
    // спринта/рывка (InputComponent — только с надетыми Boots/Ring) и доступность пистолета (WeaponComponent —
    // только с надетым Weapon-арбалетом). Публичный — зовётся и отсюда при каждой экипировке/снятии/сбросе, и
    // один раз из SceneFacade сразу после создания игрока, чтобы старт без экипировки сразу выставил ограничения.
    void recomputeEquipmentEffects();

    // Пассивная регенерация от надетого Ожерелья (см. NECKLACE_REGEN_AMOUNT/INTERVAL в .cpp) — единственная
    // категория экипировки без брони/durability, раньше просто занимала слот и ничего не делала.
    void update(sf::Time dt) override;

    void reset() override;

private:
    // Индекс первого пустого слота мешка или -1, если мешок полон.
    int findEmptySlot() const;

    int m_baseArmor;
    std::array<InventorySlot, BAG_SIZE> m_bag;
    std::map<ItemCategory, const ItemDefinition*> m_equipment;
    // Только у категорий с durability > 0 и только пока не сломались (см. absorbHit) — остальные надетые
    // категории (Ring/Neck/Weapon) сюда не попадают вовсе.
    std::map<ItemCategory, int> m_durability;
    // Накопитель времени до следующего тика регенерации от Ожерелья (см. update() в .cpp) — сбрасывается в 0,
    // когда Ожерелье снято, а не продолжает копиться "про запас".
    sf::Time m_regenElapsed;
};
