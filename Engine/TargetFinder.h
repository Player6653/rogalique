#pragma once
#include <SFML/System/Vector2.hpp>
#include <functional>

class GameObject;
class HealthComponent;
class NavGrid;

// cos(60°) — общий порог "передний конус ±60°" (120° суммарно). Раньше был продублирован как отдельная
// constexpr в анонимных namespace AttackComponent.cpp И RangedAttackComponent.cpp (второй копией со своим
// комментарием "тот же порог, что у AttackComponent") — один общий источник истины исключает риск обновить
// угол обзора в одном месте и забыть про другое.
constexpr float TARGET_CONE_DOT_THRESHOLD = 0.5f;

// Общая логика поиска ближайшей подходящей цели, вынесенная из AttackComponent::findTarget() (ближний бой) и
// RangedAttackComponent::findTargetDirection() (дальний бой) — оба перебирали GameWorld::getHealthComponents(),
// отфильтровывали себя/мёртвых/не прошедших targetFilter, проверяли дистанцию и передний конус facing одним и
// тем же способом, различаясь только диапазоном дистанции (ближний бой — [0, range], дальний — [minRange,
// maxRange]) и тем, что дальнему бою ещё нужна проверка прямой видимости через NavGrid (не стрелять сквозь
// стену), а ближнему — нет.
//
// checkCone=false — конус не проверяется вовсе (омнидирекциональная атака, например радиус вокруг слизи).
// navGridForLineOfSight=nullptr — проверка видимости пропускается (ближний бой: если цель в радиусе, она и так
// рядом, стены enemy типично не разделяют вплотную стоящих).
//
// Возвращает ближайшего подходящего (не первого зарегистрированного — см. использующие компоненты, почему это
// важно) или nullptr, если такого нет.
HealthComponent* findBestAttackTarget(GameObject* owner, sf::Vector2f facing, float facingLength, float minRange, float maxRange,
    bool checkCone, const std::function<bool(GameObject*)>& targetFilter, NavGrid* navGridForLineOfSight = nullptr);
