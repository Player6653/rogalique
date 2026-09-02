#pragma once
#include "IComponent.h"
#include <functional>
#include <string>
#include <vector>

class SpriteComponent;

// Один кадровый ролик — путь к листу, число кадров, длительность кадра, зациклен ли, и (для многострочных листов
// вроде слизи, см. SpriteComponent::loadAnimation) строка/число строк. row=0, rowCount=1 — обычный однострочный лист.
struct ActorAnimClip {
    std::string path;
    int frameCount = 1;
    sf::Time frameDuration;
    bool loop = true;
    int row = 0;
    int rowCount = 1;
};

// Один источник атаки (AttackComponent::consumeJustStarted() или RangedAttackComponent::consumeJustFired(),
// завёрнутые в лямбду владельцем при конфигурации — см. Enemy.cpp/Soldier.cpp/Slime.cpp) и один или несколько
// роликов под него: больше одного — чередуются между последовательными срабатываниями (Soldier/Orc: Attack01/02).
struct ActorAttackAnim {
    std::function<bool()> consumeJustTriggered;
    std::vector<ActorAnimClip> clips;
    // Параллельно clips, необязательно (можно оставить пустым вектором) — своя тень под конкретный клип атаки,
    // если у него другая раскадровка/силуэт (Orc: Attack02 бьёт с другой стороны, тень отдельным файлом). Пустой
    // path элемента или отсутствующий индекс — тень для этого клипа обычная (ActorAnimationConfig::normalShadow).
    std::vector<ActorAnimClip> shadowOverrides;
    sf::Time visualDuration;
};

// Полная анимационная развёртка существа. attacks — по убыванию приоритета отображения (после Hurt, перед Walk):
// первый ещё не отыгравший свой visualDuration и выигрывает показ. Death/Hurt/Walk/Idle — общий для всех как есть,
// разница только в путях/числе кадров, которые сюда и приходят.
struct ActorAnimationConfig {
    ActorAnimClip idle;
    ActorAnimClip walk;
    ActorAnimClip hurt;
    sf::Time hurtVisualDuration;
    ActorAnimClip death;
    // Один и тот же статичный кадр тени переиспользуется во всех состояниях, кроме Death — та часто хочет свою
    // (шире/иначе анимированную) тень; после Death отыгрывает обратно normalShadow.
    ActorAnimClip normalShadow;
    ActorAnimClip deathShadow;
    std::vector<ActorAttackAnim> attacks;
};

// Общая логика для Enemy/Soldier/Slime (раньше — три почти одинаковых класса): состояние по HealthComponent
// (Death необратимо > Hurt на время стана) и по ActorAttackAnim-ам (первый активный по приоритету) > Walk/Idle по
// IDirectionProvider::getMoveDirection(), флип спрайта по getFacing(). Сама анимация целиком описывается через
// ActorAnimationConfig — этому компоненту ничего специфичного про конкретное существо знать не нужно.
class ActorAnimationComponent : public IComponent {
public:
    explicit ActorAnimationComponent(ActorAnimationConfig config);

    void update(sf::Time dt) override;

    // m_currentClip нарочно не сбрасываем — очередной update() сам увидит несовпадение желаемого клипа с текущим
    // ("Death") и перезагрузит анимацию. Индексы чередующихся клипов (m_attackClipIndex) тоже не сбрасываем — так
    // было и в исходных EnemyAnimationComponent/SoldierAnimationComponent, поведение сознательно не меняем.
    void reset() override;

private:
    void applyClip(SpriteComponent& sprite, const ActorAnimClip& clip) const;
    // Перезагружает тень, только если желаемый клип реально отличается от уже загруженного (сравнение по пути) —
    // иначе тень пришлось бы перезагружать на каждую смену состояния тела (Idle/Walk/Attack и т.д.), хотя в
    // подавляющем большинстве случаев она одна и та же статичная normalShadow.
    void applyShadowIfChanged(SpriteComponent& shadow, const ActorAnimClip& clip);

    ActorAnimationConfig m_config;

    std::string m_currentClip;
    std::string m_currentShadowPath;
    bool m_flippedX = false;

    bool m_wasHurt = false;
    sf::Time m_hurtVisualTimeRemaining;

    // Параллельно m_config.attacks.
    std::vector<sf::Time> m_attackVisualTimeRemaining;
    std::vector<std::size_t> m_attackClipIndex;
};
