#pragma once
#include "EngineExport.h"
#include <SFML/Graphics.hpp>

// sf::Keyboard::isKeyPressed()/sf::Mouse::isButtonPressed() читают ГЛОБАЛЬНОЕ состояние клавиатуры/мыши всей ОС
// (GetAsyncKeyState на Windows) — оно не привязано к тому, какое окно сейчас активно. Поэтому свёрнутая или просто
// не в фокусе игра всё равно реагировала на нажатия, адресованные другому окну (баг: клавиатурный/мышиный ввод
// проходил даже в свёрнутом или неактивном окне). Эти две функции — прямая замена тем двум SFML-вызовам везде в игре: тот же
// интерфейс, но сначала проверяют RenderSystem::instance().getWindow().hasFocus() — без фокуса клавиша/кнопка
// всегда "не нажата", как и ожидает игрок.
namespace FocusedInput
{
    ENGINE_API bool isKeyPressed(sf::Keyboard::Key key);
    ENGINE_API bool isButtonPressed(sf::Mouse::Button button);
} // namespace FocusedInput
