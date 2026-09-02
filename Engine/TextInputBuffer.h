#pragma once
#include "EngineExport.h"
#include <SFML/System/Utf.hpp>
#include <vector>

// Символы, введённые с клавиатуры за текущий кадр (sf::Event::TextEntered) — Engine::run() наполняет каждый
// кадр (см. .cpp), очищая перед опросом новых событий. Нужен отдельно от обычного polling-ввода движка
// (sf::Keyboard::isKeyPressed, см. FocusedInput.h) — для текстового поля (например, ввод имени в таблицу
// рекордов, см. Rogalique/NameEntryOverlayComponent) важна раскладка/Shift/IME, а это даёт только
// sf::Event::TextEntered, polling такого не умеет в принципе. Компоненты читают charsThisFrame() из своего
// update() — тот вызывается уже после того, как Engine::run() собрал события этого кадра.
namespace TextInputBuffer
{
    ENGINE_API const std::vector<sf::Uint32>& charsThisFrame();
    ENGINE_API void pushChar(sf::Uint32 unicode);
    ENGINE_API void clear();
} // namespace TextInputBuffer
