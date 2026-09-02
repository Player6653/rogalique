#pragma once

// Общий макрос экспорта для всех классов движка: dllexport при сборке самой Engine.dll, dllimport для того, кто её подключает (например, Rogalique.exe).
#ifdef ENGINE_EXPORTS
#define ENGINE_API __declspec(dllexport)
#else
#define ENGINE_API __declspec(dllimport)
#endif
