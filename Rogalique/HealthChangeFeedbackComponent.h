#pragma once
#include "IComponent.h"

class HealthComponent;
class CameraComponent;

// Graphics: визуальный отклик на изменение HP игрока (см. docs/DESIGN_DOC.md, направление Graphics Developer) —
// опрашивает HP каждый кадр (тот же приём, что раньше был в CameraShakeOnDamageComponent, теперь расширен) и на
// уменьшение трясёт камеру + красная вспышка частиц, на увеличение (лечение зельем/ожерельем) — зелёное свечение.
// Сознательно не трогает Engine/HealthComponent колбэком "на изменение HP" ради пары косметических эффектов, чтобы
// не расширять контракт часто используемого базового класса ради единственного потребителя.
class HealthChangeFeedbackComponent : public IComponent {
public:
    HealthChangeFeedbackComponent(HealthComponent& target, CameraComponent& camera);

    void update(sf::Time dt) override;
    void reset() override;

private:
    HealthComponent& m_target;
    CameraComponent& m_camera;
    int m_lastHp;
};
