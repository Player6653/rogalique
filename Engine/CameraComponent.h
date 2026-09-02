#pragma once
#include "EngineExport.h"
#include "IComponent.h"
#include <SFML/Graphics.hpp>

// Стратегия камеры в следующем - часть окна, центр которой автоматически следует за позицией владельца. apply() вызывается явно из Engine::run() один раз за кадр, до отрисовки сцены это не часть обычного прохода GameObject::draw(), иначе то, что применяет вид,
// зависело бы от порядка  отрисовки объектов (и рисковало бы выполниться уже после того, как что-то нарисовалось поверх).
class ENGINE_API CameraComponent : public IComponent {
public:
    explicit CameraComponent(sf::Vector2f viewSize);
    // Снимает себя с реестра GameWorld (см. GameWorld::registerCamera/getActiveCamera) — тот же приём, что у
    // HealthComponent.
    ~CameraComponent() override;

    void onOwnerMoved(sf::Vector2f newPosition) override;
    // Продвигает затухание тряски (см. shake() ниже) — сама тряска не двигает владельца камеры, только смещение
    // вида поверх обычного центра, поэтому не нужна owner-у, тикает независимо от его onOwnerMoved.
    void update(sf::Time dt) override;

    void apply(sf::RenderWindow& window);

    // Ограничивает область, которую камера может показать, мировыми границами уровня — без этого на карте,
    // которая больше view (см. большие уровни в SceneFacade), у края был бы виден чёрный "невидимый мир" за
    // пределами построенного пола. Если уровень по какой-то оси МЕНЬШЕ view (влезает в экран целиком), эта ось не
    // клампится вовсе, а центрируется на середину уровня — так же, как вело себя view до всякого clamp'а.
    void setBounds(sf::FloatRect worldBounds);

    // Тряска камеры (Graphics: удар нанесён/получен) — случайное дрожание вида вокруг обычного центра, магнитудой,
    // затухающей линейно до нуля за duration. Повторный вызов, пока предыдущая тряска ещё не отыграла, просто
    // перезапускает её (не складывается) — иначе частые удары подряд быстро раскачали бы камеру до неиграбельной
    // амплитуды.
    void shake(float magnitudePixels, sf::Time duration);

private:
    sf::View m_view;
    sf::FloatRect m_bounds;
    bool m_hasBounds = false;

    // "Настоящий" центр (куда камера следует за владельцем) — отдельно от m_view.getCenter(), которая в apply()
    // ещё получает поверх сдвиг тряски; иначе каждый новый onOwnerMoved стирал бы текущий сдвиг тряски, а тряска
    // на неподвижном игроке (стан) не пересчитывалась бы вовсе, потому что onOwnerMoved тогда не зовётся.
    sf::Vector2f m_baseCenter;
    sf::Time m_shakeDuration;
    sf::Time m_shakeRemaining;
    float m_shakeMagnitude = 0.f;
    sf::Vector2f m_shakeOffset;
};
