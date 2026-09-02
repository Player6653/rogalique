#include "pch.h"
#include "InputComponent.h"
#include "FocusedInput.h"
#include <algorithm>
#include <cmath>

namespace
{
    // Во сколько раз растёт скорость, пока зажат Shift.
    constexpr float SPRINT_MULTIPLIER = 1.7f;
    // Во сколько раз растёт скорость на время рывка.
    constexpr float DASH_MULTIPLIER = 4.5f;
    const sf::Time DASH_DURATION = sf::seconds(0.18f);
    const sf::Time DASH_COOLDOWN = sf::seconds(0.6f);

    // Прыжок работает как рывок, но мягче и без перезарядки: обычный прыжок и прыжок во время спринта летят с одной и той же скоростью, просто дольше поэтому и дальше (в полтора раза, при спринте).
    // Дальность = скорость игрока (200) * JUMP_MULTIPLIER * длительность — обычный прыжок 200*1.6*0.3=96px (2
    // тайла по 48px), прыжок со спринтом 200*1.6*0.45=144px (3 тайла). Увеличено с 1/2 тайла —
    // не хватало перепрыгнуть Pit-разрывы даже со спринтом.
    constexpr float JUMP_MULTIPLIER = 1.6f;
    const sf::Time JUMP_DURATION = sf::seconds(0.3f);
    const sf::Time JUMP_DURATION_SPRINTING = sf::seconds(0.45f);

    // Выносливость (см. InputComponent::getStamina()) — тратится на спринт и рывок, копится на ходьбе/покое.
    // Полный бак спринта держит ~4с непрерывного бега (100/25), рывок стоит пятую часть бака, восстановление
    // чуть медленнее траты спринтом — полный бак с нуля копится ~5с, чтобы нельзя было спринтовать бесконечно
    // с короткими паузами.
    constexpr float STAMINA_SPRINT_DRAIN_PER_SEC = 25.f;
    constexpr float STAMINA_DASH_COST = 20.f;
    constexpr float STAMINA_REGEN_PER_SEC = 20.f;
    // Порог возврата из "выдохся" (см. m_exhausted в .h) — на нём бак должен реально накопиться, а не просто
    // оторваться от нуля: 20% бака ~1с регена, короткая, но заметная пауза перед тем, как спринт снова доступен.
    constexpr float EXHAUSTION_RECOVERY_THRESHOLD = InputComponent::STAMINA_MAX * 0.2f;
} // namespace

void InputComponent::update(sf::Time dt)
{
    if (m_dashCooldownRemaining > sf::Time::Zero) {
        m_dashCooldownRemaining -= dt;
    }

    // Направление рывка/прыжка уже зафиксировано в m_direction в момент старта.
    if (m_dashTimeRemaining > sf::Time::Zero) {
        m_dashTimeRemaining -= dt;
        return;
    }
    if (m_jumpTimeRemaining > sf::Time::Zero) {
        m_jumpTimeRemaining -= dt;
        return;
    }

    float x = 0.f;
    float y = 0.f;

    if (FocusedInput::isKeyPressed(sf::Keyboard::A) || FocusedInput::isKeyPressed(sf::Keyboard::Left))
        x -= 1.f;
    if (FocusedInput::isKeyPressed(sf::Keyboard::D) || FocusedInput::isKeyPressed(sf::Keyboard::Right))
        x += 1.f;
    if (FocusedInput::isKeyPressed(sf::Keyboard::W) || FocusedInput::isKeyPressed(sf::Keyboard::Up))
        y -= 1.f;
    if (FocusedInput::isKeyPressed(sf::Keyboard::S) || FocusedInput::isKeyPressed(sf::Keyboard::Down))
        y += 1.f;

    sf::Vector2f direction(x, y);
    float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    m_direction = (length > 0.f) ? direction / length : direction;
    if (length > 0.f) {
        m_facing = m_direction;
    }

    bool sprintKeyHeld = m_sprintEnabled && !m_sprintSuppressed
                         && (FocusedInput::isKeyPressed(sf::Keyboard::LShift) || FocusedInput::isKeyPressed(sf::Keyboard::RShift));
    // На нуле бак не просто временно недостаточен — ставим m_exhausted и держим спринт выключенным, пока бак не
    // отрастёт до EXHAUSTION_RECOVERY_THRESHOLD (не строго до максимума: иначе после каждой полной растраты
    // пришлось бы ждать полного восстановления, прежде чем снова побежать хоть на секунду). Простой "m_stamina>0"
    // без этого флага мигал бы true/false каждый кадр на нуле — регенерация за один кадр тут же поднимает бак
    // обратно выше нуля, следующий кадр снова проходит проверку, и так по кругу (баг — анимация дёргалась).
    if (m_stamina <= 0.f) {
        m_exhausted = true;
    } else if (m_exhausted && m_stamina >= EXHAUSTION_RECOVERY_THRESHOLD) {
        m_exhausted = false;
    }
    bool sprinting = sprintKeyHeld && !m_exhausted;
    if (sprinting) {
        m_direction *= SPRINT_MULTIPLIER;
        m_stamina = std::max(0.f, m_stamina - STAMINA_SPRINT_DRAIN_PER_SEC * dt.asSeconds());
    } else {
        m_stamina = std::min(STAMINA_MAX, m_stamina + STAMINA_REGEN_PER_SEC * dt.asSeconds());
    }

    if (m_movementLocked) {
        // m_facing нарочно не трогаем — дэш/прыжок ниже должны лететь в ту сторону, куда персонаж смотрел, а не в (0,0).
        m_direction = sf::Vector2f(0.f, 0.f);
    }

    bool dashKeyHeld = m_dashEnabled
                       && (FocusedInput::isKeyPressed(sf::Keyboard::LControl)
                           || FocusedInput::isKeyPressed(sf::Keyboard::RControl) || FocusedInput::isKeyPressed(sf::Keyboard::X));
    bool dashKeyPressedThisFrame = m_dashEdge.poll(dashKeyHeld);

    bool jumpKeyHeld = FocusedInput::isKeyPressed(sf::Keyboard::Space) || FocusedInput::isKeyPressed(sf::Keyboard::C);
    bool jumpKeyPressedThisFrame = m_jumpEdge.poll(jumpKeyHeld);

    if (dashKeyPressedThisFrame && m_dashCooldownRemaining <= sf::Time::Zero && m_stamina >= STAMINA_DASH_COST) {
        m_dashTimeRemaining = DASH_DURATION;
        m_dashCooldownRemaining = DASH_COOLDOWN;
        m_direction = m_facing * DASH_MULTIPLIER;
        m_stamina -= STAMINA_DASH_COST;
    } else if (jumpKeyPressedThisFrame) {
        m_jumpTimeRemaining = sprinting ? JUMP_DURATION_SPRINTING : JUMP_DURATION;
        m_direction = m_facing * JUMP_MULTIPLIER;
    }
}

void InputComponent::resyncInput()
{
    bool dashKeyHeld = FocusedInput::isKeyPressed(sf::Keyboard::LControl) || FocusedInput::isKeyPressed(sf::Keyboard::RControl)
                       || FocusedInput::isKeyPressed(sf::Keyboard::X);
    m_dashEdge.sync(dashKeyHeld);

    bool jumpKeyHeld = FocusedInput::isKeyPressed(sf::Keyboard::Space) || FocusedInput::isKeyPressed(sf::Keyboard::C);
    m_jumpEdge.sync(jumpKeyHeld);
}

void InputComponent::reset()
{
    m_direction = sf::Vector2f(0.f, 0.f);
    m_facing = sf::Vector2f(0.f, 1.f);
    m_dashEdge.sync(false);
    m_dashTimeRemaining = sf::Time::Zero;
    m_dashCooldownRemaining = sf::Time::Zero;
    m_jumpEdge.sync(false);
    m_jumpTimeRemaining = sf::Time::Zero;
    m_movementLocked = false;
    m_stamina = STAMINA_MAX;
    m_exhausted = false;
}
