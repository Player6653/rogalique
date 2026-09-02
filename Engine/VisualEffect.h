#pragma once
#include "EngineExport.h"
#include "GameObject.h"
#include <string>

// Одноразовый декоративный визуальный эффект — спрайт проигрывает clip РОВНО ОДИН РАЗ (без loop) и сам уничтожает
// себя, как только ролик доигран (см. .cpp — внутренний компонент-таймер). Ничего не двигает, ни с кем не
// сталкивается и не наносит урона — чистая картинка поверх/под сценой (взрыв на попадании снаряда, вспышка АОЕ-
// разряда и т.п., см. Boss.cpp). В отличие от ParticleSystem (общий пул кружков) показывает настоящий
// нарисованный кадровый ролик из купленного пака, когда для эффекта есть готовый спрайт-лист, а не просто цветную
// частицу.
class ENGINE_API VisualEffect : public GameObject {
public:
    // row/rowCount — как у ActorAnimClip: 0/1, если texturePath однострочный лист.
    VisualEffect(sf::Vector2f position, const std::string& texturePath, sf::Vector2f visualSize, int frameCount,
        sf::Time frameDuration, int row = 0, int rowCount = 1, float rotationDegrees = 0.f);
};
