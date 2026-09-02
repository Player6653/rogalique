#include "pch.h"
#include "HealthBarComponent.h"
#include "HealthComponent.h"
#include "Log.h"
#include <algorithm>

namespace
{
    // Опорные зубчики стоят на x=6,22,38,54,70 — ровно 4 равных отсека по 16px между ними. inset ниже это как раз x=6 слева и (77-70)=7 справа, чтобы наши деления легли точно в эти отсеки.
    constexpr float LEFT_INSET_RATIO = 6.f / 77.f;
    constexpr float RIGHT_INSET_RATIO = 7.f / 77.f;
    constexpr float VERTICAL_INSET_RATIO = 0.2f;
    // Промежуток между делениями, в экранных пикселях — отдельно от родного узора рамки (тот под произвольный maxHp не подстроить, у него фиксированное число зубчиков).
    constexpr float SEGMENT_GAP = 3.f;
} // namespace

HealthBarComponent::HealthBarComponent(
    HealthComponent& target, const std::string& texturePath, sf::Vector2f size, std::function<bool()> isVisible)
    : m_target(target),
      m_isVisible(std::move(isVisible)),
      m_size(size)
{
    m_fallbackBackground.setSize(size);
    m_fallbackBackground.setFillColor(sf::Color(20, 20, 20, 180));
    m_fallbackBackground.setOutlineColor(sf::Color(0, 0, 0, 200));
    m_fallbackBackground.setOutlineThickness(1.f);

    m_hasTexture = m_frameTexture.loadFromFile(texturePath);
    if (!m_hasTexture) {
        LOG_WARN("HealthBarComponent: не удалось загрузить текстуру \"" + texturePath + "\", останется только подложка");
        return;
    }
    m_frameSprite.setTexture(m_frameTexture, true);
    sf::Vector2u textureSize = m_frameTexture.getSize();
    m_frameSprite.setScale(m_size.x / static_cast<float>(textureSize.x), m_size.y / static_cast<float>(textureSize.y));

    rebuildSegments();
}

void HealthBarComponent::rebuildSegments()
{
    m_segments.clear();
    m_segmentLocalOffsets.clear();

    int maxHp = m_target.getMaxHp();
    m_lastKnownMaxHp = maxHp;
    if (maxHp <= 0) {
        return;
    }

    // Деления кладём внутрь заполняемой середины рамки, а не поверх декоративных краёв.
    float innerLeft = m_size.x * LEFT_INSET_RATIO;
    float innerRight = m_size.x * (1.f - RIGHT_INSET_RATIO);
    float innerWidth = innerRight - innerLeft;
    float verticalInset = m_size.y * VERTICAL_INSET_RATIO;

    float segmentWidth = (innerWidth - SEGMENT_GAP * (maxHp - 1)) / static_cast<float>(maxHp);
    segmentWidth = std::max(1.f, segmentWidth);
    for (int i = 0; i < maxHp; ++i) {
        sf::RectangleShape segment(sf::Vector2f(segmentWidth, m_size.y - verticalInset * 2.f));
        m_segments.push_back(segment);
        m_segmentLocalOffsets.emplace_back(innerLeft + i * (segmentWidth + SEGMENT_GAP), verticalInset);
    }
    // Новые сегменты только что созданы без позиции (RectangleShape по умолчанию на (0,0)) — onOwnerMoved()
    // применит owner-позицию + офсеты на следующем кадре, но update()/draw() этого же кадра увидят их на (0,0),
    // если не выставить сразу здесь.
    for (std::size_t i = 0; i < m_segments.size(); ++i) {
        m_segments[i].setPosition(m_ownerPosition + m_segmentLocalOffsets[i]);
    }
}

void HealthBarComponent::update(sf::Time dt)
{
    if (m_target.getMaxHp() != m_lastKnownMaxHp) {
        rebuildSegments();
    }

    int hp = m_target.getHp();
    for (std::size_t i = 0; i < m_segments.size(); ++i) {
        bool alive = static_cast<int>(i) < hp;
        // Живое деление — яркая заливка; потерянное — полностью прозрачное, сквозь него видно полупрозрачную середину самой рамки.
        m_segments[i].setFillColor(alive ? sf::Color(235, 60, 90, 235) : sf::Color(0, 0, 0, 0));
    }
}

void HealthBarComponent::draw(sf::RenderWindow& window) const
{
    if (m_isVisible && !m_isVisible()) {
        return;
    }

    if (!m_hasTexture) {
        window.draw(m_fallbackBackground);
        return;
    }

    window.draw(m_frameSprite);
    for (const auto& segment : m_segments) {
        window.draw(segment);
    }
}

void HealthBarComponent::onOwnerMoved(sf::Vector2f newPosition)
{
    m_ownerPosition = newPosition;
    m_fallbackBackground.setPosition(newPosition);
    m_frameSprite.setPosition(newPosition);
    for (std::size_t i = 0; i < m_segments.size(); ++i) {
        m_segments[i].setPosition(newPosition + m_segmentLocalOffsets[i]);
    }
}
