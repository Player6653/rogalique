#pragma once
#include "EngineExport.h"
#include "Logger.h"
#include <memory>
#include <string>
#include <unordered_map>

// Центральное хранилище именованных логгеров — по имени либо отдаёт уже созданный Logger, либо создаёт новый.
class ENGINE_API LoggerRegistry {
public:
    static LoggerRegistry& getInstance();

    LoggerRegistry(const LoggerRegistry&) = delete;
    LoggerRegistry& operator=(const LoggerRegistry&) = delete;

    Logger& getLogger(const std::string& name);

private:
    LoggerRegistry() = default;

    std::unordered_map<std::string, std::unique_ptr<Logger>> m_loggers;
};
