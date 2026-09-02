#pragma once
#include <string>

// Разрешение окна и полноэкранный режим — в отличие от остальных настроек (звук/лимит FPS, см. SettingsOverlayComponent
// в run()), применяются не мгновенно, а только при следующем запуске процесса. Причина: смена разрешения означает
// пересоздание окна, а все оверлеи (MenuOverlayComponent/HUD/InventoryOverlayComponent и т.д.) и камера игрока
// считают свою раскладку/размер вида один раз при сборке сцены под конкретные windowWidth/windowHeight (см.
// SceneFacade::run()) — живой пересчёт этой раскладки при каждой смене разрешения ради настройки, которую меняют
// раз в десять игровых сессий, не оправдан. Поэтому выбор просто сохраняется на диск, а следующий запуск читает
// его до создания окна (см. RenderSystem::createWindow).
struct DisplaySettings {
    int width = 1248;
    int height = 768;
    bool fullscreen = false;
    // Лимит FPS (0 — без ограничения) и громкость музыки/эффектов [0..1] — в отличие от width/height/fullscreen
    // выше, применяются мгновенно (см. run()), но раньше вообще не сохранялись на диск: слайдеры настроек меняли
    // только живые RenderSystem/AudioSystem, а при следующем запуске те снова создавались с дефолтами (лимит 0,
    // громкость 1.0) — отсюда баг "лимит FPS и звук сбиваются после перезапуска".
    unsigned fpsLimit = 0;
    float musicVolume = 1.f;
    float effectsVolume = 1.f;

    void save(const std::string& filePath) const;
    // Значения по умолчанию (см. поля выше), если файла нет или он повреждён — не ошибка, а первый запуск.
    static DisplaySettings load(const std::string& filePath);
};

// Тот же принцип, что и savePath()/saveFileExists() в SavePaths.h.
inline const char* displaySettingsPath()
{
    return "display_settings.txt";
}
