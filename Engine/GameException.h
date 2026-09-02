#pragma once
#include <stdexcept>
#include <string>

// Базовое исключение движка для ошибок, из которых можно восстановиться на вызывающей стороне (неверные параметры компонента, не поднявшееся окно и т.п.).
class GameException : public std::runtime_error {
public:
    explicit GameException(const std::string& message)
        : std::runtime_error(message)
    {
    }
};
