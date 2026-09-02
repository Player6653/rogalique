#include "pch.h"
#include "Logger.h"

Logger::Logger(std::string name)
    : m_name(std::move(name))
{
}

void Logger::addSink(std::unique_ptr<LogSink> sink)
{
    m_sinks.push_back(std::move(sink));
}

void Logger::info(const std::string& message)
{
    log(LogLevel::Info, message);
}

void Logger::warning(const std::string& message)
{
    log(LogLevel::Warning, message);
}

void Logger::error(const std::string& message)
{
    log(LogLevel::Error, message);
}

void Logger::log(LogLevel level, const std::string& message)
{
    for (auto& sink : m_sinks) {
        sink->write(level, m_name, message);
    }
}
