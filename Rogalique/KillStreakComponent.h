#pragma once
#include "IComponent.h"
#include <functional>

class HealthComponent;

// Один раз, в момент когда HealthComponent владельца переходит в isDead()==true, зовёт onKilled — используется для
// награды "убил 5 ботов подряд в подземелье -> +1 максимум HP" (см. SceneFacade.cpp). Вешается только на
// "настоящих" врагов подземелья — НЕ на детей деления слизи и НЕ на волны арены (награда не должна работать на
// арене, а дети деления — не самостоятельная угроза, а бонус за убийство и без того уже засчитанного Slime2).
class KillStreakComponent : public IComponent {
public:
    KillStreakComponent(HealthComponent& health, std::function<void()> onKilled);

    void update(sf::Time dt) override;
    void reset() override;

    // Синхронизирует m_wasDead с ФАКТИЧЕСКИМ текущим isDead(), не вызывая onKilled — нужна загрузке сохранения
    // (см. SceneFacade.cpp): resetComponents() сначала оживляет владельца (сбрасывая m_wasDead в false через
    // обычный reset()), а следом отдельным вызовом HealthComponent::setHp() владельцу возвращают HP из сейва — и
    // если он на самом деле был мёртв ещё до сохранения, HP снова станет 0. Без этой синхронизации ПОСЛЕ setHp()
    // update() увидел бы "только что оживили — только что снова умер" как настоящее убийство и засчитал бы
    // награду за уже давно убитого бота при каждой перезагрузке сейва (был баг — фарм +1 maxHP).
    void syncToCurrentState();

private:
    HealthComponent& m_health;
    std::function<void()> m_onKilled;
    bool m_wasDead = false;
};
