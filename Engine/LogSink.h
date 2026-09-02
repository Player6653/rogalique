#pragma once
#include "LogLevel.h"
#include <string>

// Единый интерфейс приёмника логов.
class LogSink {
public:
    virtual ~LogSink() = default;

    virtual void write(LogLevel level, const std::string& loggerName, const std::string& message) = 0;
};
