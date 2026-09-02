#pragma once
#include <fstream>

// Единое место, куда игра сохраняется/из которого загружается.
inline const char* savePath()
{
    return "savegame.txt";
}

// Есть ли файл сохранения прямо сейчас.
inline bool saveFileExists()
{
    std::ifstream file(savePath());
    return file.is_open();
}

// Таблица рекордов прохождения (см. Leaderboard.h) — отдельный файл от сейва, переживает "В главное меню"/новые
// забеги (сейв удаляется/перезаписывается, рекорды копятся).
inline const char* highscoresPath()
{
    return "highscores.txt";
}
