#pragma once
#include "EngineExport.h"
#include "IComponent.h"
#include "IDirectionProvider.h"
#include "InputEdge.h"
#include <SFML/Graphics.hpp>

// Стратегия ввода опрашивает клавиатуру (WASD и стрелки) и превращает нажатые клавиши в вектор направления.
class ENGINE_API InputComponent : public IComponent, public IDirectionProvider {
public:
    void update(sf::Time dt) override;
    void reset() override;

    // (0,0), если ничего не нажато; иначе вектор длины 1 (диагональ не быстрее движения по одной оси), умноженный на SPRINT_MULTIPLIER, пока зажат Shift, либо на DASH_MULTIPLIER/JUMP_MULTIPLIER на время рывка/прыжка.
    sf::Vector2f getMoveDirection() const override
    {
        return m_direction;
    }
    // В отличие от getMoveDirection() не обнуляется, пока игрок стоит на месте — держит последнее ненулевое направление ходьбы, поэтому атака бьёт туда, куда игрок смотрит, даже без движения.
    sf::Vector2f getFacing() const override
    {
        return m_facing;
    }

    // true всё то время, пока длится рывок нужно PlayerAnimationComponent, чтобы показать ролик Dash.
    bool isDashing() const
    {
        return m_dashTimeRemaining > sf::Time::Zero;
    }
    // true всё то время, пока длится прыжок нужно PlayerAnimationComponent, чтобы показать ролик Jump.
    bool isJumping() const
    {
        return m_jumpTimeRemaining > sf::Time::Zero;
    }

    // Рывок пробивает стан (см. IDirectionProvider::isUninterruptible) — прыжок нет, у него нет неуязвимости, это просто перемещение, а не способ вырваться из-под удара.
    bool isUninterruptible() const override
    {
        return isDashing();
    }
    // Прыжок (в отличие от рывка) перепрыгивает яму/лаву (см. IDirectionProvider::ignoresObstacles, PitComponent).
    bool ignoresObstacles() const override
    {
        return isJumping();
    }

    // На время удара копьём (WeaponComponent) движение WASD выключено — анимации "идёт и одновременно бьёт" в
    // паке нет, без этого персонаж визуально стоял бы в стойке замаха, а на деле скользил по экрану. Рывок/прыжок
    // при этом всё равно работают — это защитный манёвр, отменять удар им можно.
    void setMovementLocked(bool locked)
    {
        m_movementLocked = locked;
    }

    // Спринт/рывок открывает экипировка (см. InventoryComponent::recomputeEquipmentEffects — Сапоги/Кольцо
    // соответственно), а не доступны игроку по умолчанию. false здесь — Shift/Ctrl двигают как обычная ходьба.
    void setSprintEnabled(bool enabled)
    {
        m_sprintEnabled = enabled;
    }
    void setDashEnabled(bool enabled)
    {
        m_dashEnabled = enabled;
    }

    // На время перезарядки пистолета (WeaponComponent) Shift временно не ускоряет — анимации "бежит и одновременно
    // перезаряжает" в паке нет (есть только Walk_while_Reloading), без этого персонаж двигался бы на скорости
    // спринта, а анимация показывала бы обычную ходьбу. В отличие от setMovementLocked само движение не
    // выключается, только множитель спринта — перезаряжаться на бегу всё ещё можно, просто медленнее.
    void setSprintSuppressed(bool suppressed)
    {
        m_sprintSuppressed = suppressed;
    }

    // Синхронизирует "клавиша уже была зажата" (m_dashEdge/m_jumpEdge) с реальным состоянием клавиатуры прямо
    // сейчас. Звать сразу после того, как мир выходит из паузы (GameWorld::setPaused(false)): пока мир на паузе,
    // update() не вызывается вовсе, и клавиша, которой только что подтвердили пункт меню (например, Space —
    // теперь тоже "подтвердить" в MenuOverlayComponent), на первом кадре читалась бы как "только что нажали", и
    // рывок/прыжок срабатывал бы без реального нового нажатия.
    void resyncInput();

    // Выносливость — общий ресурс спринта и рывка (не прыжка, тот отдельно описан как "просто перемещение", а
    // не боевой манёвр, см. ignoresObstacles() выше). Тратится, пока держишь спринт, и разово за рывок;
    // восстанавливается, когда не спринтуешь. HUD читает эти два геттера, чтобы нарисовать полоску.
    float getStamina() const
    {
        return m_stamina;
    }
    static constexpr float STAMINA_MAX = 100.f;

private:
    sf::Vector2f m_direction;
    // Последнее ненулевое направление ходьбы — рывок/прыжок на месте (без зажатых клавиш движения) летит лицом сюда.
    sf::Vector2f m_facing = sf::Vector2f(0.f, 1.f);

    KeyEdge m_dashEdge;
    sf::Time m_dashTimeRemaining;
    sf::Time m_dashCooldownRemaining;

    KeyEdge m_jumpEdge;
    sf::Time m_jumpTimeRemaining;

    bool m_movementLocked = false;
    bool m_sprintEnabled = true;
    bool m_dashEnabled = true;
    bool m_sprintSuppressed = false;

    float m_stamina = STAMINA_MAX;
    // Гистерезис на "выдохся" (см. .cpp): без него регенерация в тот же кадр, что бак дошёл до нуля, тут же
    // возвращает его выше 0, и на следующем кадре спринт снова проходит проверку — мигание true/false каждый кадр,
    // игрок фактически не перестаёт бежать, а анимация дёргается между Walk/Run на каждом кадре (был баг).
    bool m_exhausted = false;
};
