#pragma once
#include "EngineExport.h"
#include "IComponent.h"

// Метка "этот объект появился в сцене динамически во время игры, а не через изначальную расстановку актёров в
// SceneFacade" — например, дети деления слизи (см. Rogalique/SlimeSplitComponent). Сам по себе ничего не делает,
// только флаг для GameObject::destroyTransientChildren(): полный ребут уровня ("Начать"/"В главное меню"/
// "Загрузить сохранение") сбрасывает изначальный ростер актёров через resetComponents(), но ничего не знает про
// объекты, заспавненные позже в процессе игры, — без этой метки такие объекты пережили бы рестарт и остались
// бы висеть на локации.
class ENGINE_API TransientComponent : public IComponent {};
