#include "Leaderboard.h"
#include <algorithm>
#include <fstream>

void Leaderboard::load(const std::string& filePath)
{
    m_entries.clear();

    std::ifstream file(filePath);
    if (!file.is_open()) {
        return; // Файла ещё нет (первый запуск/ни разу не прошли игру) — не ошибка.
    }

    std::string name;
    int timeSeconds = 0;
    while (file >> name >> timeSeconds) {
        m_entries.push_back({name, timeSeconds});
    }

    sortByTimeAscending();
}

bool Leaderboard::save(const std::string& filePath) const
{
    std::ofstream file(filePath);
    if (!file.is_open()) {
        return false;
    }
    for (const Entry& entry : m_entries) {
        file << entry.name << " " << entry.timeSeconds << "\n";
    }
    return true;
}

void Leaderboard::addEntry(const std::string& name, int timeSeconds)
{
    // Формат файла — "имя secondsInt" через пробел, читается через whitespace-delimited operator>> (см. load()
    // выше) — имя с пробелом расшатало бы разбор этой и последующих строк. Сейчас единственный источник имени
    // (NameEntryInput::isAllowedChar) сам исключает пробел, но это невидимая зависимость между двумя разными
    // файлами в разных каталогах — экранируем на всякий случай здесь же, а не полагаемся на неё.
    std::string safeName = name;
    std::replace(safeName.begin(), safeName.end(), ' ', '_');
    m_entries.push_back({safeName, timeSeconds});
    sortByTimeAscending();
}

void Leaderboard::sortByTimeAscending()
{
    std::sort(m_entries.begin(), m_entries.end(), [](const Entry& a, const Entry& b) { return a.timeSeconds < b.timeSeconds; });
}
