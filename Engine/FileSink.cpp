#include "pch.h"
#include "FileSink.h"
#include <fstream>

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

FileSink::FileSink(std::string filePath)
    : m_filePath(std::move(filePath))
{
}

void FileSink::write(LogLevel level, const std::string& loggerName, const std::string& message)
{
    // ofstream открывается и закрывается (в деструкторе при выходе из области видимости) на каждую запись сообщение гарантированно на диске сразу после этого вызова.
    std::ofstream file(m_filePath, std::ios::app);
    if (!file.is_open()) {
        return;
    }
    file << "[" << loggerName << "] [" << levelToString(level) << "] " << message << "\n";
}
