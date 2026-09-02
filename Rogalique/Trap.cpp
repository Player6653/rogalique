#include "Trap.h"
#include "SpriteComponent.h"
#include "TrapComponent.h"

namespace
{
    // В 2 раза больше тайла (48x48 -> 96x96) — на маленьком спрайте шипы было плохо видно и легко не заметить.
    const sf::Vector2f VISUAL_SIZE(96.f, 96.f);
    // Resources/Map/spike/spike.png — 432x48, 9 кадров 48x48: первые 3 (точки) убраны, последние 6 (полные шипы)
    // выдвинуты (проверено по картинке пака).
    constexpr int FRAME_COUNT = 9;
    const sf::Time FRAME_DURATION = sf::seconds(0.12f);
    const sf::Time CYCLE_DURATION = sf::seconds(0.12f * FRAME_COUNT);
    const sf::Time DANGEROUS_PHASE_START = sf::seconds(0.12f * 3.f);
} // namespace

Trap::Trap(sf::Vector2f position)
    : GameObject(position)
{
    SpriteComponent& icon = addComponent<SpriteComponent>(VISUAL_SIZE);
    icon.setPlaceholderColor(sf::Color(150, 60, 60));
    icon.loadAnimation("Resources/Map/spike/spike.png", FRAME_COUNT, FRAME_DURATION, true);

    addComponent<TrapComponent>(CYCLE_DURATION, DANGEROUS_PHASE_START);
}
