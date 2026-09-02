#include "DisplaySettings.h"
#include <fstream>

void DisplaySettings::save(const std::string& filePath) const
{
    std::ofstream file(filePath);
    if (!file.is_open()) {
        return; // Некуда сохранить — следующий запуск просто возьмёт значения по умолчанию, не смертельно.
    }
    file << width << " " << height << " " << (fullscreen ? 1 : 0) << " " << fpsLimit << " " << musicVolume << " " << effectsVolume
         << "\n";
}

DisplaySettings DisplaySettings::load(const std::string& filePath)
{
    DisplaySettings result;
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return result;
    }
    int width = 0;
    int height = 0;
    int fullscreenFlag = 0;
    if (file >> width >> height >> fullscreenFlag && width > 0 && height > 0) {
        result.width = width;
        result.height = height;
        result.fullscreen = fullscreenFlag != 0;
    }
    // Поля новее (fpsLimit/громкость) — файл со старой сессии их просто не содержит, тогда чтение молча
    // проваливается и остаются значения по умолчанию из полей структуры, а не бросается ошибка.
    unsigned fpsLimit = 0;
    float musicVolume = 0.f;
    float effectsVolume = 0.f;
    if (file >> fpsLimit >> musicVolume >> effectsVolume) {
        result.fpsLimit = fpsLimit;
        result.musicVolume = musicVolume;
        result.effectsVolume = effectsVolume;
    }
    return result;
}
