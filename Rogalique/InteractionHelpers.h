#pragma once
#include <SFML/System/Vector2.hpp>

class GameObject;
class KeyEdge;

// Общий приём интерактивных объектов на карте — открыть/подобрать по клавише E, только пока игрок физически
// рядом (Chest/Door/ItemPickupComponent): ищет игрока (см. findChaseTarget() в Engine/ChaseTargetComponent.h),
// синхронизирует interactEdge клавиши E, пока игрока не нашли (сама попытка poll() без реального игрока в сцене
// не имеет смысла), считает дистанцию до него от owner и опрашивает интерактивным edge-детектором клавишу E
// каждый кадр, пока игрок вообще существует (не только пока в радиусе — иначе E, зажатая ещё на подходе,
// засчиталась бы "только что нажали" в первый же кадр, когда игрок реально войдёт в радиус, тот же класс бага,
// что чинит KeyEdge::sync() в других местах). Возвращает true, только если игрок одновременно в радиусе И только
// что нажал E. outPlayer, если не nullptr, получает найденного игрока (нужен вызывающему для доступа к его
// компонентам — InventoryComponent и т.п.) даже тогда, когда сам вызов вернул false из-за того, что E не нажата
// (но не тогда, когда игрока не нашли вовсе — тогда *outPlayer не трогается).
//
// Раньше этот же ~15-строчный блок (findChaseTarget + расчёт дистанции + poll/sync interactEdge) был скопирован
// почти дословно в ChestComponent::update() и DoorComponent::update() — отличались только именем константы
// дистанции.
bool isPlayerInRangeAndInteractPressed(
    GameObject& owner, float interactDistance, KeyEdge& interactEdge, GameObject** outPlayer = nullptr);
