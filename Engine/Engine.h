#ifndef ENGINE_H
#define ENGINE_H
#include "EngineExport.h"

// Синглтон единственная точка входа в игровой цикл движка. Опирается на другие синглтоны RenderSystem (окно) и GameWorld (дерево объектов).
class ENGINE_API Engine {
public:
    static Engine& instance();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    // Классический игровой цикл.
    void run();

    // Останавливает run() на следующей итерации (например, по игровому событию выход).
    void stop();

private:
    Engine() = default;

    bool m_running = false;
};
#endif // !ENGINE_H
