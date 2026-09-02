#pragma once
#include "EngineExport.h"

// Суммарная прокрутка колеса мыши за текущий кадр (sf::Event::MouseWheelScrolled) — Engine::run() наполняет каждый
// кадр (см. .cpp), очищая перед опросом новых событий. Тот же приём, что и у TextInputBuffer.h — обычный polling
// (sf::Mouse::...) для колеса не годится в принципе, у него нет состояния "зажато", только разовые события
// прокрутки. Компоненты читают deltaThisFrame() из своего update() — тот вызывается уже после того, как
// Engine::run() собрал события этого кадра.
namespace MouseWheelBuffer
{
    ENGINE_API float deltaThisFrame();
    ENGINE_API void pushDelta(float delta);
    ENGINE_API void clear();
} // namespace MouseWheelBuffer
