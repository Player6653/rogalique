#pragma once
#include "EngineExport.h"
#include "IComponent.h"
#include <SFML/Graphics.hpp>

class SpriteComponent;

// Плавное проявление спрайта после спавна — сразу ставит нулевую прозрачность (см. конструктор) и за duration
// линейно доводит её до полной, вместо того чтобы объект резко появлялся на экране целиком за один кадр (волны
// арены раньше появлялись именно так, см. SceneFacade.cpp). Общий по духу приём с
// HitFlashComponent (тоже гоняет SpriteComponent::setColor/clearColor по таймеру), но противоположная цель —
// проявление, а не мигание, и ровно один раз, не мигает туда-обратно.
class ENGINE_API SpawnFadeComponent : public IComponent {
public:
    SpawnFadeComponent(SpriteComponent& sprite, sf::Time duration);

    void update(sf::Time dt) override;
    void reset() override;

private:
    SpriteComponent& m_sprite;
    sf::Time m_duration;
    sf::Time m_elapsed;
    bool m_finished = false;
};
