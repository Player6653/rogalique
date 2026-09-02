#include "Log.h"
#include "SceneFacade.h"

int main()
{
    // Последний рубеж, если что-то исключение долетело аж досюда, не подхваченное точечными try/catch внутри (SceneFacade, HealthComponent) — логируем и выходим с кодом ошибки вместо необработанного краша с диалогом ОС.
    try {
        SceneFacade sceneFacade;
        sceneFacade.run();
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("main: необработанное исключение, завершаю работу: ") + e.what());
        return 1;
    }
    return 0;
}
