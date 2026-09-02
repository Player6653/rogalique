#include "GameMemento.h"
#include <fstream>

namespace
{
    // "-" вместо itemId у пустого слота/категории — id предметов никогда не пустые и не состоят из одного дефиса,
    // так что сентинел однозначно отличим от настоящего id при чтении.
    const std::string EMPTY_ITEM_TOKEN = "-";
} // namespace

bool GameMemento::save(const std::string& filePath) const
{
    std::ofstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    file << m_playerPosition.x << " " << m_playerPosition.y << " " << m_playerHp << "\n";
    file << m_enemyPosition.x << " " << m_enemyPosition.y << " " << m_enemyHp << "\n";
    // Каждая следующая необязательная запись пишется, только если есть данные и для неё, и для всех предыдущих —
    // иначе при чтении её было бы невозможно отличить от полей объекта, добавленного позже, но отсутствующего в
    // конкретном сейве (load() читает их строго по порядку, первая же неудачная останавливает всю цепочку).
    if (m_soldier.hasData) {
        file << m_soldier.position.x << " " << m_soldier.position.y << " " << m_soldier.hp << "\n";
        if (m_slime1.hasData) {
            file << m_slime1.position.x << " " << m_slime1.position.y << " " << m_slime1.hp << "\n";
            if (m_slime2.hasData) {
                file << m_slime2.position.x << " " << m_slime2.position.y << " " << m_slime2.hp << "\n";
                if (m_slime3.hasData) {
                    file << m_slime3.position.x << " " << m_slime3.position.y << " " << m_slime3.hp << "\n";
                    if (m_inventory.hasData) {
                        file << m_inventory.bag.size() << "\n";
                        for (const BagSlotSave& slot : m_inventory.bag) {
                            file << (slot.itemId.empty() ? EMPTY_ITEM_TOKEN : slot.itemId) << " " << slot.count << "\n";
                        }
                        file << m_inventory.equipment.size() << "\n";
                        for (const EquipSlotSave& slot : m_inventory.equipment) {
                            file << slot.category << " " << (slot.itemId.empty() ? EMPTY_ITEM_TOKEN : slot.itemId) << " "
                                 << slot.durability << "\n";
                        }
                        file << m_inventory.soldierArrows << " " << m_inventory.slime3ShotsFired << "\n";
                        file << m_inventory.collectedPickups.size() << "\n";
                        for (int collected : m_inventory.collectedPickups) {
                            file << collected << " ";
                        }
                        file << "\n";
                        if (m_inventory.hasLevelSeed) {
                            file << m_inventory.levelSeed << "\n";
                            if (m_inventory.hasExtraEnemies) {
                                file << m_inventory.extraEnemies.size() << "\n";
                                for (const ExtraEnemySave& extra : m_inventory.extraEnemies) {
                                    file << extra.position.x << " " << extra.position.y << " " << extra.hp << "\n";
                                }
                                if (m_inventory.hasLevelShapeSeed) {
                                    file << m_inventory.levelShapeSeed << "\n";
                                    if (m_inventory.hasArenaWave) {
                                        file << m_inventory.arenaWave << "\n";
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return true;
}

bool GameMemento::load(const std::string& filePath, GameMemento& outMemento)
{
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    GameMemento memento;
    if (!(file >> memento.m_playerPosition.x >> memento.m_playerPosition.y >> memento.m_playerHp)) {
        return false;
    }
    if (!(file >> memento.m_enemyPosition.x >> memento.m_enemyPosition.y >> memento.m_enemyHp)) {
        return false;
    }
    // Необязательные строки — у файлов, сохранённых до появления Soldier/Slime(2/3)/инвентаря, их просто нет, и
    // это не повод считать весь файл повреждённым (hasXData() != true, вызывающий код сам решит, что делать).
    // Каждая следующая читается, только если предыдущая была — см. симметричный комментарий в save().
    if (file >> memento.m_soldier.position.x >> memento.m_soldier.position.y >> memento.m_soldier.hp) {
        memento.m_soldier.hasData = true;
        if (file >> memento.m_slime1.position.x >> memento.m_slime1.position.y >> memento.m_slime1.hp) {
            memento.m_slime1.hasData = true;
            if (file >> memento.m_slime2.position.x >> memento.m_slime2.position.y >> memento.m_slime2.hp) {
                memento.m_slime2.hasData = true;
                if (file >> memento.m_slime3.position.x >> memento.m_slime3.position.y >> memento.m_slime3.hp) {
                    memento.m_slime3.hasData = true;

                    std::size_t bagCount = 0;
                    if (file >> bagCount) {
                        std::vector<BagSlotSave> bag;
                        bag.reserve(bagCount);
                        bool bagOk = true;
                        for (std::size_t i = 0; i < bagCount && bagOk; ++i) {
                            BagSlotSave slot;
                            if (!(file >> slot.itemId >> slot.count)) {
                                bagOk = false;
                                break;
                            }
                            if (slot.itemId == EMPTY_ITEM_TOKEN) {
                                slot.itemId.clear();
                            }
                            bag.push_back(std::move(slot));
                        }

                        std::size_t equipCount = 0;
                        if (bagOk && (file >> equipCount)) {
                            std::vector<EquipSlotSave> equipment;
                            equipment.reserve(equipCount);
                            bool equipOk = true;
                            for (std::size_t i = 0; i < equipCount && equipOk; ++i) {
                                EquipSlotSave slot;
                                if (!(file >> slot.category >> slot.itemId >> slot.durability)) {
                                    equipOk = false;
                                    break;
                                }
                                if (slot.itemId == EMPTY_ITEM_TOKEN) {
                                    slot.itemId.clear();
                                }
                                equipment.push_back(std::move(slot));
                            }

                            int soldierArrows = 0;
                            int slime3ShotsFired = 0;
                            if (equipOk && (file >> soldierArrows >> slime3ShotsFired)) {
                                std::size_t pickupCount = 0;
                                if (file >> pickupCount) {
                                    std::vector<int> collectedPickups;
                                    collectedPickups.reserve(pickupCount);
                                    bool pickupsOk = true;
                                    for (std::size_t i = 0; i < pickupCount && pickupsOk; ++i) {
                                        int collected = 0;
                                        if (!(file >> collected)) {
                                            pickupsOk = false;
                                            break;
                                        }
                                        collectedPickups.push_back(collected);
                                    }
                                    if (pickupsOk) {
                                        memento.setInventoryData(std::move(bag), std::move(equipment), soldierArrows,
                                            slime3ShotsFired, std::move(collectedPickups));
                                        // Отдельный необязательный хвост — сейв мог быть записан этой же сессией
                                        // до появления этого поля; если его нет, всё остальное (bag/equipment/
                                        // collectedPickups) уже успешно применено через setInventoryData() выше.
                                        unsigned levelSeed = 0;
                                        if (file >> levelSeed) {
                                            memento.setLevelSeed(levelSeed);
                                            std::size_t extraCount = 0;
                                            if (file >> extraCount) {
                                                std::vector<ExtraEnemySave> extras;
                                                extras.reserve(extraCount);
                                                bool extrasOk = true;
                                                for (std::size_t i = 0; i < extraCount && extrasOk; ++i) {
                                                    ExtraEnemySave extra;
                                                    if (!(file >> extra.position.x >> extra.position.y >> extra.hp)) {
                                                        extrasOk = false;
                                                        break;
                                                    }
                                                    extras.push_back(extra);
                                                }
                                                if (extrasOk) {
                                                    memento.setExtraEnemies(std::move(extras));
                                                    // Отдельный необязательный хвост (после extraEnemies) — та же
                                                    // логика, что и у остальных выше: сейв мог быть записан этой
                                                    // же сессией до появления пересборки формы уровня.
                                                    unsigned levelShapeSeed = 0;
                                                    if (file >> levelShapeSeed) {
                                                        memento.setLevelShapeSeed(levelShapeSeed);
                                                        // Последнее звено цепочки — см. симметричный комментарий у
                                                        // extraEnemies выше: сейв мог быть записан этой же сессией
                                                        // до появления сохранения состояния арены волн.
                                                        int arenaWave = -1;
                                                        if (file >> arenaWave) {
                                                            memento.setArenaWave(arenaWave);
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    outMemento = memento;
    return true;
}
