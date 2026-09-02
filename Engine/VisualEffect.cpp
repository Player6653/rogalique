#include "pch.h"
#include "VisualEffect.h"
#include "SpriteComponent.h"

namespace
{
    // Считает duration и уничтожает владельца — приватный для этого файла, никто больше не заводит "поиграл и
    // сам себя убрал" объекты через готовый ролик (обычные частицы всё ещё через ParticleSystem, см. класс-
    // комментарий VisualEffect.h), отдельный публичный класс ради одного пользователя избыточен.
    class TimedDestroyComponent : public IComponent {
    public:
        explicit TimedDestroyComponent(sf::Time duration)
            : m_remaining(duration)
        {
        }

        void update(sf::Time dt) override
        {
            m_remaining -= dt;
            if (m_remaining <= sf::Time::Zero) {
                GameObject* owner = getOwner();
                if (owner) {
                    owner->destroy();
                }
            }
        }

    private:
        sf::Time m_remaining;
    };
} // namespace

VisualEffect::VisualEffect(sf::Vector2f position, const std::string& texturePath, sf::Vector2f visualSize, int frameCount,
    sf::Time frameDuration, int row, int rowCount, float rotationDegrees)
    : GameObject(position)
{
    SpriteComponent& sprite = addComponent<SpriteComponent>(visualSize);
    sprite.loadAnimation(texturePath, frameCount, frameDuration, /*loop=*/false, row, rowCount);
    sprite.setRotation(rotationDegrees);
    // +1 кадр запаса — ролик не зациклен (loop=false, см. выше), последний кадр иначе мог бы мигнуть и пропасть
    // на том же кадре обновления, где ещё должен был быть виден.
    addComponent<TimedDestroyComponent>(frameDuration * static_cast<float>(frameCount + 1));
}
