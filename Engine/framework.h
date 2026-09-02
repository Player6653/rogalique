#pragma once

#define WIN32_LEAN_AND_MEAN // Исключите редко используемые компоненты из заголовков Windows.
#define NOMINMAX            // windows.h не должен определять min/max они конфликтуют со std::min/std::max в SFML.
// Файлы заголовков Windows.
#include <windows.h>
