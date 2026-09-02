#include "pch.h"
#include "ConsoleSink.h"
#include <iostream>

namespace
{
    const char* levelToString(LogLevel level)
    {
        switch (level) {
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warning:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
        }
        return "?";
    }
} // namespace

void ConsoleSink::write(LogLevel level, const std::string& loggerName, const std::string& message)
{
    std::ostream& out = (level == LogLevel::Error) ? std::cerr : std::cout;
    out << "[" << loggerName << "] [" << levelToString(level) << "] " << message << std::endl;
}
