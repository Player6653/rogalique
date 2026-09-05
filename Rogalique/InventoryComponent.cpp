#include "InventoryComponent.h"
#include "AudioSystem.h"
#include "GameObject.h"
#include "HealthComponent.h"
#include "InputComponent.h"
#include "Log.h"
#include "WeaponComponent.h"
#include <algorithm>
#include <cassert>

namespace
{
    bool isEquipCategory(ItemCategory category)
    {
        return category != ItemCategory::Consumable && category != ItemCategory::Misc;
    }

    // Ожерелье (см. ItemCategory::Neck) — единственная категория экипировки без брони/durability (см.
    // ItemDefinition.cpp) — раньше вообще ничего не делала, просто занимала слот. Пассивная
    // регенерация, пока надета: раз в NECKLACE_REGEN_INTERVAL, если владелец жив и HP не полное, +1 HP. 1 — то же,
    // что у Малого зелья лечения (см. ItemDefinition.cpp "potion_small") — при максимум 4 HP у игрока (см.
    // PLAYER_MAX_HP в Player.cpp) более сильный тик ощущался бы читерским пассивным вечным хилом.
    constexpr int NECKLACE_REGEN_AMOUNT = 1;
    // Было 4с, замедлено до 6с — 4с ощущалось слишком щедрым пассивным хилом на фоне
    // PLAYER_MAX_HP=4 (по сути полный автохил из ямы за один цикл боя).
    const sf::Time NECKLACE_REGEN_INTERVAL = sf::seconds(6.f);

    // Порядок, в котором ломающаяся броня по очереди принимает на себя удары (см. InventoryComponent::absorbHit) —
    // НЕ совпадает с порядком слотов в UI (см. EQUIP_CATEGORY_ORDER в SceneFacade.cpp, тот — просто раскладка
    // сетки инвентаря). Щит первым как самая живучая и самая "активная" защита, шлем последним как самая хрупкая.
    const ItemCategory DURABILITY_ORDER[] = {ItemCategory::Shield, ItemCategory::Chest, ItemCategory::Pants, ItemCategory::Head};
} // namespace

InventoryComponent::InventoryComponent(int baseArmor)
    : m_baseArmor(baseArmor)
{
    assert(baseArmor >= 0 && "InventoryComponent: baseArmor must not be negative");
}

int InventoryComponent::findEmptySlot() const
{
    for (int i = 0; i < BAG_SIZE; ++i) {
        if (m_bag[i].isEmpty()) {
            return i;
        }
    }
    return -1;
}

bool InventoryComponent::addItem(const ItemDefinition& item, int count, int durability)
{
    // Всё-или-ничего: сперва считаем суммарную вместимость (незаполненные стеки того же предмета + пустые слоты),
    // ничего не трогая. Без этой предварительной проверки при нехватке места под ВЕСЬ count функция раньше могла
    // успеть добавить только часть, но всё равно вернуть false — вызывающий (ItemPickupComponent) считал бы
    // предмет не подобранным и попробовал бы добавить его снова целиком при следующем подборе, задвоив уже
    // добавленную часть.
    int available = 0;
    for (const InventorySlot& slot : m_bag) {
        if (slot.isEmpty()) {
            available += item.maxStack;
        } else if (slot.item == &item) {
            available += item.maxStack - slot.count;
        }
        if (available >= count) {
            break;
        }
    }
    if (available < count) {
        LOG_WARN("InventoryComponent: мешок полон, не удалось добавить \"" + item.displayName + "\"");
        return false;
    }

    // Вместимость уже подтверждена — сначала доливаем существующие стеки, остаток гарантированно поместится в
    // пустые слоты.
    for (InventorySlot& slot : m_bag) {
        if (count <= 0) {
            break;
        }
        if (slot.item == &item && slot.count < item.maxStack) {
            int spaceLeft = item.maxStack - slot.count;
            int added = std::min(spaceLeft, count);
            slot.count += added;
            count -= added;
        }
    }
    while (count > 0) {
        int index = findEmptySlot();
        int added = std::min(count, item.maxStack);
        m_bag[index] = InventorySlot{&item, added, durability};
        count -= added;
    }
    return true;
}

bool InventoryComponent::removeItemById(const std::string& itemId, int count)
{
    bool removedAny = false;
    for (InventorySlot& slot : m_bag) {
        if (count <= 0) {
            break;
        }
        if (slot.isEmpty() || slot.item->id != itemId) {
            continue;
        }
        int take = std::min(count, slot.count);
        slot.count -= take;
        count -= take;
        removedAny = true;
        if (slot.count <= 0) {
            slot = InventorySlot{};
        }
    }
    return removedAny;
}

void InventoryComponent::useBagSlot(int index)
{
    if (index < 0 || index >= BAG_SIZE || m_bag[index].isEmpty()) {
        return;
    }
    InventorySlot& slot = m_bag[index];
    const ItemDefinition& item = *slot.item;

    if (item.category == ItemCategory::Consumable) {
        GameObject* owner = getOwner();
        auto* health = owner ? owner->getComponent<HealthComponent>() : nullptr;
        // HP уже полное — не тратим зелье впустую (клик не должен списывать расходник без эффекта).
        if (health && health->getHp() >= health->getMaxHp()) {
            LOG_INFO("Inventory: HP уже полное, \"" + item.displayName + "\" не использовано");
            return;
        }
        if (health) {
            health->setHp(health->getHp() + item.healAmount);
        }
        --slot.count;
        if (slot.count <= 0) {
            slot = InventorySlot{};
        }
        LOG_INFO("Inventory: использовано \"" + item.displayName + "\"");
        return;
    }

    if (isEquipCategory(item.category)) {
        const ItemDefinition* previous = getEquipped(item.category);
        int previousDurability = getDurability(item.category);
        // Прочность именно этого экземпляра — 0, если предмет только что подобран с карты/сундука и ещё ни разу
        // не был надет (тогда ниже берём полную ItemDefinition::durability), иначе то, что от него осталось после
        // прошлой носки (см. unequip()).
        int incomingDurability = slot.durability;
        // Слот, откуда взяли надеваемый предмет, освобождаем и сразу пробуем вернуть в него прежнюю экипировку —
        // не отдельным addItem() "в никуда", иначе один и тот же предмет мог бы задвоиться, если мешок как раз
        // оказался полон в этот момент.
        InventorySlot savedSlot = slot;
        --slot.count;
        if (slot.count <= 0) {
            slot = InventorySlot{};
        }
        // Если возвращать некуда (мешок полон и без места даже под только что освобождённый слот — с текущими
        // предметами такого не бывает, вся экипировка maxStack==1, но код не должен молча на этом полагаться) —
        // отменяем всю замену, а не надеваем новое поверх старого: иначе прежняя экипировка терялась бы
        // безвозвратно (найдено при аудите инвентаря).
        if (previous && !addItem(*previous, 1, previousDurability)) {
            slot = savedSlot;
            LOG_WARN("Inventory: мешок полон, \"" + item.displayName + "\" не надето — некуда вернуть \""
                      + previous->displayName + "\"");
            return;
        }
        m_equipment[item.category] = &item;
        if (item.durability > 0) {
            m_durability[item.category] = incomingDurability > 0 ? incomingDurability : item.durability;
        } else {
            m_durability.erase(item.category);
        }
        recomputeEquipmentEffects();
        AudioSystem::instance().playSound("equip");
        LOG_INFO("Inventory: надето \"" + item.displayName + "\"");
        return;
    }

    // Misc — просто лежит в мешке, использовать нечем (пока не появится, например, механика замков под ключи).
}

void InventoryComponent::unequip(ItemCategory category)
{
    const ItemDefinition* equipped = getEquipped(category);
    if (!equipped) {
        return;
    }
    // Прочность уходит в мешок вместе с предметом (см. InventorySlot::durability) — иначе следующее надевание
    // выдавало бы её заново полной (см. useBagSlot()).
    if (!addItem(*equipped, 1, getDurability(category))) {
        LOG_WARN("Inventory: мешок полон, не удалось снять \"" + equipped->displayName + "\"");
        return;
    }
    m_equipment.erase(category);
    m_durability.erase(category);
    recomputeEquipmentEffects();
    AudioSystem::instance().playSound("equip");
    LOG_INFO("Inventory: снято \"" + equipped->displayName + "\"");
}

void InventoryComponent::forceEquip(ItemCategory category, const ItemDefinition& item, int durability)
{
    m_equipment[category] = &item;
    if (durability > 0) {
        m_durability[category] = durability;
    } else {
        m_durability.erase(category);
    }
}

const ItemDefinition* InventoryComponent::getEquipped(ItemCategory category) const
{
    auto it = m_equipment.find(category);
    return it == m_equipment.end() ? nullptr : it->second;
}

int InventoryComponent::getDurability(ItemCategory category) const
{
    auto it = m_durability.find(category);
    return it == m_durability.end() ? 0 : it->second;
}

int InventoryComponent::getTotalDurability() const
{
    int total = 0;
    for (const auto& entry : m_durability) {
        total += entry.second;
    }
    return total;
}

int InventoryComponent::absorbHit(int incomingDamage)
{
    // Броня — вторая шкала HP, а не "блокирует N ударов целиком": из каждой категории по очереди вычитаем ровно
    // столько очков прочности, сколько нанёс удар (не 1 заряд за удар) — сильный удар может сразу и доломать
    // текущую категорию, и каскадом задеть следующую по DURABILITY_ORDER, если "не поместился" целиком.
    int remaining = incomingDamage;
    for (ItemCategory category : DURABILITY_ORDER) {
        if (remaining <= 0) {
            break;
        }
        auto it = m_durability.find(category);
        if (it == m_durability.end() || it->second <= 0) {
            continue;
        }
        int consumed = std::min(it->second, remaining);
        it->second -= consumed;
        remaining -= consumed;
        if (it->second <= 0) {
            const ItemDefinition* broken = getEquipped(category);
            LOG_INFO("Inventory: \"" + (broken ? broken->displayName : std::string()) + "\" сломан(а) от удара и уничтожен(а)");
            m_equipment.erase(category);
            m_durability.erase(category);
            recomputeEquipmentEffects();
        }
    }
    return remaining;
}

void InventoryComponent::recomputeEquipmentEffects()
{
    GameObject* owner = getOwner();
    if (!owner) {
        return;
    }

    if (auto* health = owner->getComponent<HealthComponent>()) {
        int bonus = 0;
        for (const auto& entry : m_equipment) {
            bonus += entry.second->armorBonus;
        }
        health->setArmor(m_baseArmor + bonus);
    }
    if (auto* input = owner->getComponent<InputComponent>()) {
        input->setSprintEnabled(getEquipped(ItemCategory::Boots) != nullptr);
        input->setDashEnabled(getEquipped(ItemCategory::Ring) != nullptr);
    }
    if (auto* weapon = owner->getComponent<WeaponComponent>()) {
        weapon->setGunAvailable(getEquipped(ItemCategory::Weapon) != nullptr);
    }
}

void InventoryComponent::reset()
{
    m_bag.fill(InventorySlot{});
    m_equipment.clear();
    m_durability.clear();
    m_regenElapsed = sf::Time::Zero;
    recomputeEquipmentEffects();
}

void InventoryComponent::update(sf::Time dt)
{
    if (!getEquipped(ItemCategory::Neck)) {
        // Не копим время, пока ожерелье снято — надели заново, тик начинается с нуля, а не "доигрывает" старый.
        m_regenElapsed = sf::Time::Zero;
        return;
    }
    m_regenElapsed += dt;
    if (m_regenElapsed < NECKLACE_REGEN_INTERVAL) {
        return;
    }
    m_regenElapsed -= NECKLACE_REGEN_INTERVAL;

    GameObject* owner = getOwner();
    auto* health = owner ? owner->getComponent<HealthComponent>() : nullptr;
    if (health && !health->isDead() && health->getHp() < health->getMaxHp()) {
        health->setHp(health->getHp() + NECKLACE_REGEN_AMOUNT);
    }
}
