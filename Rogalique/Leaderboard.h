#pragma once
#include <string>
#include <vector>

// Таблица рекордов прохождения — портировано из старого Arkanoid-проекта этого автора (см. титры), с поправкой
// на то, что тут рекорд не "очки, больше — лучше", а "время в секундах, меньше — лучше" (см.
// sortByTimeAscending()).
class Leaderboard {
public:
    struct Entry {
        std::string name;
        int timeSeconds = 0;
    };

    // Читает записи из текстового файла в m_entries. Отсутствующий/битый файл — пустая таблица, не ошибка
    // (первый запуск).
    void load(const std::string& filePath);

    // Записывает обратно в файл в том же формате ("имя секунды", по строке на запись).
    bool save(const std::string& filePath) const;

    // Добавляет новую запись и пересортировывает m_entries по возрастанию времени (быстрее — выше).
    void addEntry(const std::string& name, int timeSeconds);

    const std::vector<Entry>& getEntries() const
    {
        return m_entries;
    }

private:
    void sortByTimeAscending();

    std::vector<Entry> m_entries;
};
