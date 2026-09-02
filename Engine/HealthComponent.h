#pragma once
#include "EngineExport.h"
#include "IComponent.h"
#include <SFML/System/Time.hpp>
#include <algorithm>
#include <functional>

class GameObject;

// Здоровье и броня объекта.
class ENGINE_API HealthComponent : public IComponent {
public:
    // postHitInvulnerability — необязательные i-frames: после КАЖДОГО реально прошедшего удара (не отбитого
    // существующей неуязвимостью) сразу включает неуязвимость на этот срок. 0 (по умолчанию) — отключено, как
    // раньше; нужно игроку, чтобы несколько ботов не выкашивали HP одной пачкой почти одновременных ударов.
    HealthComponent(int maxHp, int armor, sf::Time postHitInvulnerability = sf::Time::Zero);
    // Снимает себя с реестра GameWorld.
    ~HealthComponent() override;

    void update(sf::Time dt) override;
    // Полное HP, без брони/неуязвимости/стана — для рестарта матча.
    void reset() override;

    // Возвращает реально снятые HP.
    int takeDamage(int amount);
    // Мгновенно обнуляет HP, минуя броню/неуязвимость — не боевой урон, а самостоятельная смерть (например, слизь,
    // расстрелявшая весь боезапас, см. SlimeShotLimitComponent). В отличие от takeDamage(большое число) намеренно
    // игнорирует i-frames — те про "не умереть от урона извне", а не про "не смочь умереть вовсе".
    void kill()
    {
        m_hp = 0;
    }
    bool isDead() const
    {
        return m_hp <= 0;
    }
    int getHp() const
    {
        return m_hp;
    }
    // Прижимает hp к [0, maxHp] — нужен загрузке сохранения (GameMemento), где значение приходит из внешнего файла и не обязано быть в допустимых границах.
    void setHp(int hp);
    int getMaxHp() const
    {
        return m_maxHp;
    }
    int getArmor() const
    {
        return m_armor;
    }
    // Динамическая броня нужна экипировке (InventoryComponent) — снаряжение может добавлять/убирать бонус к
    // броне во время игры, в отличие от конструктора, который задаёт её лишь единожды при создании.
    void setArmor(int armor)
    {
        m_armor = std::max(0, armor);
    }

    // Необязательный перехватчик входящего урона — зовётся первым в takeDamage(), ещё до вычета брони.
    // Возвращает урон, который пойдёт дальше (0 — удар поглощён целиком). Rogalique вешает сюда расходуемую
    // прочность экипировки (см. InventoryComponent::absorbHit) — это НЕ постоянный armor-стат, а "блокирует N
    // ударов целиком, потом ломается". nullptr по умолчанию — ничего не меняет, как раньше.
    void setDamageInterceptor(std::function<int(int)> interceptor)
    {
        m_damageInterceptor = std::move(interceptor);
    }

    // На duration игнорирует takeDamage — нужно для и фреймов.
    void setInvulnerable(sf::Time duration);
    bool isInvulnerable() const
    {
        return m_invulnerableTimeRemaining > sf::Time::Zero;
    }

    // Кратковременный стан на каждый реально прошедший удар.
    bool isStunned() const
    {
        return m_stunTimeRemaining > sf::Time::Zero;
    }

    // Публичный доступ к владельцу нужен AttackComponent, чтобы найти коллайдер цели по HealthComponent.
    GameObject* getOwner() const
    {
        return IComponent::getOwner();
    }

private:
    int m_hp;
    int m_maxHp;
    int m_armor;
    sf::Time m_invulnerableTimeRemaining;
    sf::Time m_stunTimeRemaining;
    sf::Time m_postHitInvulnerability;
    std::function<int(int)> m_damageInterceptor;
};
