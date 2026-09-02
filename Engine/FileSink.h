#pragma once
#include "EngineExport.h"
#include "LogSink.h"
#include <string>

// Пишет каждое сообщение отдельной операцией открытие-запись-закрытие, а не держит файл открытым всё время работы программы.
class ENGINE_API FileSink : public LogSink {
public:
    explicit FileSink(std::string filePath);

    void write(LogLevel level, const std::string& loggerName, const std::string& message) override;

private:
    std::string m_filePath;
};
