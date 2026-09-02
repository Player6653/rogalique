#pragma once
#include "IComponent.h"
#include <string>

class GameObject;

// Слизь-делитель (Slime2, см. SlimeConfig::canSplit в Slime.h): когда владелец умирает и доигрывает анимацию
// смерти, спавнит childCount маленьких копий той же расцветки рядом. У самих детей canSplit=false — иначе цепочка
// расщепления была бы бесконечной, см. конструктор Slime, вызывающий этот компонент, в Slime.cpp.
class SlimeSplitComponent : public IComponent {
public:
    // childSkin/childMaxHp/childVisualScale/childSpeed/childDetectionRadius — параметры детей (Slime.cpp сам
    // решает, какими их делать, этот компонент только спавнит). spawnParent — куда добавлять (см.
    // GameWorld::spawnIn), обычно тот же Y-sort контейнер actors, что и у владельца. splitDelay — задержка после
    // смерти (примерно длительность ролика Death), чтобы дети не появлялись поверх ещё идущей анимации.
    SlimeSplitComponent(std::string childSkin, int childMaxHp, float childVisualScale, float childSpeed,
        float childDetectionRadius, GameObject& spawnParent, int childCount, float spreadRadius, sf::Time splitDelay);

    void update(sf::Time dt) override;
    void reset() override;

private:
    std::string m_childSkin;
    int m_childMaxHp;
    float m_childVisualScale;
    float m_childSpeed;
    float m_childDetectionRadius;
    GameObject& m_spawnParent;
    int m_childCount;
    float m_spreadRadius;
    sf::Time m_splitDelay;

    sf::Time m_delayRemaining;
    bool m_wasDead = false;
    bool m_hasSplit = false;
};
