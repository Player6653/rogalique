#pragma once
#include <string>

class GameObject;
class HealthComponent;

// Общий приём конструкторов Enemy/Soldier/Slime: HealthComponent сам проверяет maxHp/armor и кидает GameException
// на некорректные значения — здесь эта проверка ловится и подменяется безопасными дефолтами (1/0), чтобы одна
// опечатка в константах баланса не роняла всю игру. Раньше был скопирован дословно (try/catch/LOG_ERROR) в каждом
// из трёх конструкторов, отличаясь только строкой classLabel для сообщения в лог.
HealthComponent& addHealthComponentWithFallback(GameObject& owner, int maxHp, int armor, const std::string& classLabel);
