#include "pch.h"
#include "SpriteComponent.h"
#include "Log.h"
#include <algorithm>
#include <map>

namespace
{
    // Общий на всю игру кэш текстур по пути файла — на большой карте сотни/тысячи тайлов ссылаются на считанные
    // десятки уникальных картинок; без кэша каждый SpriteComponent грузил бы и держал в видеопамяти свою отдельную
    // копию одного и того же файла (так и было раньше — работало, но не масштабировалось). Ссылки на элементы
    // std::map не инвалидируются при добавлении новых элементов (только при удалении конкретного элемента, а из
    // этого кэша ничего не удаляется за время жизни процесса), поэтому хранить у SpriteComponent просто указатель
    // на запись в кэше безопасно.
    std::map<std::string, sf::Texture>& textureCache()
    {
        static std::map<std::string, sf::Texture> cache;
        return cache;
    }
} // namespace

SpriteComponent::SpriteComponent(sf::Vector2f size)
    : m_size(size)
{
    m_placeholder.setSize(size);
    m_placeholder.setOrigin(size.x / 2.f, size.y / 2.f);
}

void SpriteComponent::setPlaceholderColor(sf::Color color)
{
    m_placeholder.setFillColor(color);
    m_placeholderBaseColor = color;
}

void SpriteComponent::setFlippedX(bool flipped)
{
    m_flippedX = flipped;
    m_sprite.setScale(m_flippedX ? -m_scale : m_scale, m_scaleY);
}

void SpriteComponent::setRotation(float degrees)
{
    m_sprite.setRotation(degrees);
    m_placeholder.setRotation(degrees);
}

void SpriteComponent::setColor(sf::Color color)
{
    m_sprite.setColor(color);
    m_placeholder.setFillColor(color);
}

void SpriteComponent::clearColor()
{
    m_sprite.setColor(sf::Color::White);
    m_placeholder.setFillColor(m_placeholderBaseColor);
}

bool SpriteComponent::loadTexture(const std::string& path)
{
    return loadAnimation(path, 1, sf::Time::Zero);
}

const sf::Texture* SpriteComponent::getCachedTexture(const std::string& path)
{
    auto& cache = textureCache();
    auto it = cache.find(path);
    if (it == cache.end()) {
        // operator[] дефолт-конструирует sf::Texture на месте, в самом кэше — без временной копии/перемещения.
        sf::Texture& texture = cache[path];
        if (!texture.loadFromFile(path)) {
            cache.erase(path);
            LOG_WARN("SpriteComponent: не удалось загрузить текстуру \"" + path + "\", остаётся заглушка");
            return nullptr;
        }
        it = cache.find(path);
    }
    return &it->second;
}

bool SpriteComponent::loadTextureRegion(const std::string& path, sf::IntRect rect, bool repeat)
{
    const sf::Texture* texture = getCachedTexture(path);
    if (!texture) {
        m_hasTexture = false;
        m_texture = nullptr;
        return false;
    }
    if (repeat) {
        // Текстура общая на всю игру (см. кэш в getCachedTexture) — setRepeated(true) не портит ничьё другое
        // использование той же картинки: он влияет только на то, ЧТО делает сэмплер за пределами обычного [0,w)x
        // [0,h) прямоугольника, а обычные (не repeat) вызовы всегда берут rect строго внутри этих границ, так что
        // им разницы нет. const_cast оправдан: сам объект в кэше не const, наружу отдаётся const только чтобы
        // рядовые вызовы SpriteComponent случайно не мутировали чужую текстуру.
        const_cast<sf::Texture*>(texture)->setRepeated(true);
    }
    m_texture = texture;
    m_hasTexture = true;

    m_frameCount = 1;
    m_frameWidth = rect.width;
    m_frameHeight = rect.height;
    m_row = 0;
    m_frameDuration = sf::Time::Zero;
    m_frameElapsed = sf::Time::Zero;
    m_currentFrame = 0;
    m_loop = false;

    m_sprite.setTexture(*m_texture, true);
    m_sprite.setOrigin(m_frameWidth / 2.f, m_frameHeight / 2.f);
    m_scale = m_size.x / static_cast<float>(m_frameWidth);
    m_scaleY = m_size.y / static_cast<float>(m_frameHeight);
    m_sprite.setScale(m_flippedX ? -m_scale : m_scale, m_scaleY);
    m_sprite.setTextureRect(rect);

    return true;
}

bool SpriteComponent::loadAnimation(
    const std::string& path, int frameCount, sf::Time frameDuration, bool loop, int row, int rowCount, bool preserveAspect)
{
    const sf::Texture* texture = getCachedTexture(path);
    if (!texture) {
        m_hasTexture = false;
        m_texture = nullptr;
        return false;
    }
    m_texture = texture;
    m_hasTexture = true;

    m_frameCount = frameCount > 0 ? frameCount : 1;
    m_frameWidth = static_cast<int>(m_texture->getSize().x) / m_frameCount;
    m_frameHeight = static_cast<int>(m_texture->getSize().y) / (rowCount > 0 ? rowCount : 1);
    m_row = row;
    m_frameDuration = frameDuration;
    m_frameElapsed = sf::Time::Zero;
    m_currentFrame = 0;
    m_loop = loop;

    m_sprite.setTexture(*m_texture, true);
    m_sprite.setOrigin(m_frameWidth / 2.f, m_frameHeight / 2.f);
    if (preserveAspect) {
        // Один общий множитель — вписываем кадр в size с сохранением пропорций (не заполняя целиком, если кадр не
        // квадратный), а не растягиваем оси по отдельности. Центрируется само — origin уже поставлен в центр кадра.
        m_scale = std::min(m_size.x / static_cast<float>(m_frameWidth), m_size.y / static_cast<float>(m_frameHeight));
        m_scaleY = m_scale;
    } else {
        // X и Y — независимо: для подавляющего большинства спрайтов size уже подобран под пропорции кадра, так что
        // оба множителя совпадают и результат как раньше (без искажений); но когда это не так (см. m_scaleY в .h,
        // почему) — растягиваем каждую ось под свою цель, а не по одной ширине с сохранением пропорций.
        m_scale = m_size.x / static_cast<float>(m_frameWidth);
        m_scaleY = m_size.y / static_cast<float>(m_frameHeight);
    }
    m_sprite.setScale(m_flippedX ? -m_scale : m_scale, m_scaleY);
    applyFrame(0);

    return true;
}

void SpriteComponent::applyFrame(int frameIndex)
{
    m_sprite.setTextureRect(sf::IntRect(frameIndex * m_frameWidth, m_row * m_frameHeight, m_frameWidth, m_frameHeight));
}

void SpriteComponent::update(sf::Time dt)
{
    if (!m_hasTexture || m_frameCount <= 1 || m_frameDuration == sf::Time::Zero) {
        return;
    }

    if (!m_loop && m_currentFrame >= m_frameCount - 1) {
        return;
    }

    m_frameElapsed += dt;
    while (m_frameElapsed >= m_frameDuration) {
        m_frameElapsed -= m_frameDuration;
        if (m_loop) {
            m_currentFrame = (m_currentFrame + 1) % m_frameCount;
        } else if (m_currentFrame < m_frameCount - 1) {
            ++m_currentFrame;
        } else {
            break;
        }
        applyFrame(m_currentFrame);
    }
}

void SpriteComponent::draw(sf::RenderWindow& window) const
{
    // Отсечение вне текущего вида (камеры для мира, дефолтного для HUD — см. Engine::run()): GameObject::draw()
    // рекурсивно рисует ВООБЩЕ ВСЁ дерево сцены каждый кадр, ничего не пропуская (см. GameObject.cpp) — пока
    // уровень целиком помещался в одно окно, это было не важно, рисовали ровно то, что и так видно. Большая карта
    // (Rogalique — SceneFacade::buildBigLevelLayout) на порядок больше окна: без отсечения каждый кадр честно
    // рисовались бы все ~4000 тайлов пола/стен уровня разом, а не полтысячи реально видимых — просадка FPS была
    // ровно отсюда, не из логики. Немного увеличиваем прямоугольник вида (на треть тайла) на всякий случай, чтобы
    // объект не мигал, если её края совпадают тик в тик.
    const sf::View& view = window.getView();
    sf::Vector2f viewCenter = view.getCenter();
    sf::Vector2f viewSize = view.getSize();
    constexpr float MARGIN = 16.f;
    sf::FloatRect viewBounds(viewCenter.x - viewSize.x / 2.f - MARGIN, viewCenter.y - viewSize.y / 2.f - MARGIN,
        viewSize.x + MARGIN * 2.f, viewSize.y + MARGIN * 2.f);

    if (m_hasTexture) {
        if (!viewBounds.intersects(m_sprite.getGlobalBounds())) {
            return;
        }
        window.draw(m_sprite);
    } else {
        if (!viewBounds.intersects(m_placeholder.getGlobalBounds())) {
            return;
        }
        window.draw(m_placeholder);
    }
}

void SpriteComponent::onOwnerMoved(sf::Vector2f newPosition)
{
    m_placeholder.setPosition(newPosition);
    m_sprite.setPosition(newPosition);
}
