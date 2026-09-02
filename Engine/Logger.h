#pragma once
#include "EngineExport.h"
#include "LogSink.h"
#include <memory>
#include <string>
#include <vector>

// Центральный пункт логирования одного именованного канала.
class ENGINE_API Logger {
public:
    explicit Logger(std::string name);

    // Logger владеет своими синками через unique_ptr (как GameObject компонентами), поэтому сам не копируется.
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // Logger владеет синком (unique_ptr) — как GameObject владеет своими компонентами.
    void addSink(std::unique_ptr<LogSink> sink);

    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);

private:
    void log(LogLevel level, const std::string& message);

    std::string m_name;
    std::vector<std::unique_ptr<LogSink>> m_sinks;
};
