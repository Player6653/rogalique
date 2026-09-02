#include "pch.h"
#include "FocusedInput.h"
#include "RenderSystem.h"

namespace FocusedInput
{
    bool isKeyPressed(sf::Keyboard::Key key)
    {
        return RenderSystem::instance().getWindow().hasFocus() && sf::Keyboard::isKeyPressed(key);
    }

    bool isButtonPressed(sf::Mouse::Button button)
    {
        return RenderSystem::instance().getWindow().hasFocus() && sf::Mouse::isButtonPressed(button);
    }
} // namespace FocusedInput
