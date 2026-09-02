#pragma once
#include "EngineExport.h"
#include "LogSink.h"

// Пишет каждое сообщение в стандартный поток вывода Error — в stderr, Info/Warning — в stdout.
class ENGINE_API ConsoleSink : public LogSink {
public:
    void write(LogLevel level, const std::string& loggerName, const std::string& message) override;
};
