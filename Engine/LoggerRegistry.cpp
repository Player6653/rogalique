#include "pch.h"
#include "LoggerRegistry.h"

LoggerRegistry& LoggerRegistry::getInstance()
{
    static LoggerRegistry instance;
    return instance;
}

Logger& LoggerRegistry::getLogger(const std::string& name)
{
    auto it = m_loggers.find(name);
    if (it == m_loggers.end()) {
        it = m_loggers.emplace(name, std::make_unique<Logger>(name)).first;
    }
    return *it->second;
}
