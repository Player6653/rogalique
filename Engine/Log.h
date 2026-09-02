#pragma once
#include "LoggerRegistry.h"

// Однострочные вызовы логирования в общий канал global.
#define LOG_INFO(message) LoggerRegistry::getInstance().getLogger("global").info(message)
#define LOG_WARN(message) LoggerRegistry::getInstance().getLogger("global").warning(message)
#define LOG_ERROR(message) LoggerRegistry::getInstance().getLogger("global").error(message)
